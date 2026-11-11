# Unattended unlock in CI and deployment

This page shows how to unlock a git-crypt repository on a machine where
nobody can type a passphrase or copy a key file by hand: a CI job, a deploy
script, a container build.

There are two approaches:

1. [A symmetric key stored as a CI secret](#option-1-a-symmetric-key-from-a-ci-secret)
   (simplest; works anywhere).
2. [A dedicated GPG deploy key](#option-2-a-dedicated-gpg-deploy-key)
   (no key file; fits setups that already use `add-gpg-user`).

## Security cautions: read this first

> **The key you give to CI decrypts every version of every file that it
> protects, including the whole Git history.**  Anyone who obtains the key
> (a leaked CI secret, a compromised runner, a malicious pull request
> workflow) can decrypt all past and future commits that use that key, and
> removing a file from the latest commit does not help.  git-crypt cannot
> revoke a key that has already leaked; the only remedy is to create a new
> key, re-encrypt, and rotate the secrets themselves.

Treat the CI key like the most sensitive credential in the repository, and:

- **Use a dedicated key for deployment-only secrets.**  Give CI a named key
  (`git-crypt init -k deploy`) that protects only the files CI needs, so a
  leak does not expose everything else.  See
  [Using a dedicated key](#using-a-dedicated-key-for-deployment-only-secrets).
- **Scope the CI secret as narrowly as you can.**  On GitHub, store it as an
  *environment* secret (Settings, Environments) with required reviewers
  and/or a deployment-branch restriction, rather than a repository-wide
  secret that every workflow, branch and fork-derived job could read.
  Never expose it to workflows triggered by untrusted pull requests.
- **Never print the key.**  Do not `echo` it, do not run the job with
  `set -x` or debug logging while it is in scope, and do not interpolate
  secrets into `run:` scripts (`${{ secrets.X }}` inside a script body);
  pass them through `env:` instead.  Secret masking only covers the exact
  stored value, not derived values such as a decoded or re-encoded copy.
- **Do not leave the key on disk.**  Prefer piping the key to
  `git-crypt unlock -` (nothing is written to a file you have to delete),
  run the job on an ephemeral runner, and lock the repository when you are
  done (`git-crypt lock --if-unlocked`).  Locking removes the unlocked key
  from `.git/git-crypt/keys/`.
- **Remember that decrypted files are plaintext on the runner.**  Do not
  upload the working tree as a build artifact or cache entry after
  unlocking.

## Option 1: a symmetric key from a CI secret

### 1. Export the key once, on a trusted machine

`export-key` accepts `-` as the file name and writes the key to standard
output.  The key is binary, so base64-encode it to store it as text:

```sh
git-crypt export-key - | base64
```

(For a named key: `git-crypt export-key -k deploy - | base64`.)

Paste the single line of output into your CI system as a secret, for
example `GIT_CRYPT_KEY_B64`.  Avoid writing the key to a file; if you do
(`git-crypt export-key /path/to/key`), delete it afterwards.

`base64` flags differ between systems; see
[base64 portability](#base64-portability).

### 2. Decode and unlock in the job

`git-crypt unlock -` reads the key from standard input, so the key never
touches the disk:

```sh
echo "$GIT_CRYPT_KEY_B64" | base64 -d | git-crypt unlock -
```

The repository must be a real Git clone, with a clean working tree; see
[Requirements and pitfalls](#requirements-and-pitfalls).

### Example: GitHub Actions

```yaml
name: deploy
on:
  push:
    branches: [main]

jobs:
  deploy:
    runs-on: ubuntu-latest
    # An environment lets you add required reviewers and branch rules to the
    # secret.  Store GIT_CRYPT_KEY_B64 as an *environment* secret here.
    environment: production
    steps:
      - uses: actions/checkout@v7
        # Default is fetch-depth: 1 (a shallow clone).  That is fine for
        # git-crypt; no history is needed to unlock.

      - name: Install git-crypt
        run: sudo apt-get update && sudo apt-get install -y git-crypt
        # Distribution packages may be older than this version of git-crypt
        # and lack 'lock --if-unlocked' (used in the Lock step below) and
        # 'status -r'.  To use them, build git-crypt from source (INSTALL.md).

      - name: Unlock
        env:
          GIT_CRYPT_KEY_B64: ${{ secrets.GIT_CRYPT_KEY_B64 }}
        run: |
          echo "$GIT_CRYPT_KEY_B64" | base64 -d | git-crypt unlock -

      - name: Deploy
        run: ./deploy.sh   # reads the decrypted files

      - name: Lock
        if: always()
        run: git-crypt lock --if-unlocked
```

Notes on the example:

- The secret reaches the step through `env:`, not through an inline
  `${{ secrets.X }}` inside `run:`.  Inline interpolation pastes the value
  into the generated script text, which is the place where quoting bugs and
  script injection happen.
- `ubuntu-latest` uses GNU `base64`, where `-d` decodes.  On a macOS runner
  `base64 -d` also works on current systems (see
  [base64 portability](#base64-portability)).
- `git-crypt lock --if-unlocked` succeeds even if the unlock step never ran
  or failed, so it is safe in an `if: always()` step.  On GitHub-hosted
  runners the VM is discarded anyway; the lock step matters on self-hosted
  or reused runners.
- The `apt-get` package may be older than this version of git-crypt.
  `lock --if-unlocked` and `status -r` need this version; with an older
  git-crypt, use `git-crypt lock || true` in the cleanup step.
- This workflow has not been run on GitHub Actions; its `run` steps were
  simulated locally with a `file://` clone.

### Using a dedicated key for deployment-only secrets

Create a second key that only protects what CI needs.  The default key stays
on developers' machines and is never given to CI:

```sh
git-crypt init -k deploy
```

Point deployment-only files at the `deploy` filter in `.gitattributes`
(note the `git-crypt-deploy` name, in both attributes):

```
config/deploy.env filter=git-crypt-deploy diff=git-crypt-deploy
```

Export just that key for CI:

```sh
git-crypt export-key -k deploy - | base64
```

With only the `deploy` key, the files encrypted with the default key stay
encrypted (as binary data) in the CI checkout; only the `deploy` files are
decrypted.  See [multiple keys](multiple_keys.md) for more.

Because the repository then still has a locked key, `git-crypt status -r`
exits with status 1 even when the `deploy` key is unlocked.  Look at the
per-key output instead of the exit status when you use more than one key.

## Option 2: a dedicated GPG deploy key

If you prefer not to ship a raw git-crypt key, give the CI machine its own
GPG key and add it as a collaborator.  The repository then contains a copy of
the git-crypt key encrypted to that GPG key, and `git-crypt unlock` (with no
arguments) uses the GPG secret key on the machine.

On a trusted machine, create a key with no passphrase and add it to the
repository (use `-k deploy` to limit it to the deploy key):

```sh
gpg --batch --pinentry-mode loopback --passphrase '' \
    --quick-generate-key 'CI Deploy <deploy@example.com>' ed25519 cert 0
FPR=$(gpg --list-keys --with-colons deploy@example.com | awk -F: '/^fpr/{print $10; exit}')
gpg --batch --pinentry-mode loopback --passphrase '' \
    --quick-add-key "$FPR" cv25519 encr 0

git-crypt add-gpg-user --trusted "$FPR"          # or: -k deploy --trusted "$FPR"
git push
```

(`--trusted` skips GPG's web-of-trust check; see `git-crypt help add-gpg-user`.)

Export the secret key, base64-encode it, and store it as a CI secret
(`GPG_DEPLOY_KEY_B64`):

```sh
gpg --armor --export-secret-keys "$FPR" | base64
```

In the job, import it into the runner's GPG home and unlock:

```yaml
      - name: Unlock
        env:
          GPG_DEPLOY_KEY_B64: ${{ secrets.GPG_DEPLOY_KEY_B64 }}
        run: |
          echo "$GPG_DEPLOY_KEY_B64" | base64 -d | gpg --batch --import
          git-crypt unlock
```

The imported key does not need to be trusted for `git-crypt unlock` to
work.  Remove the secret key from the runner afterwards if the runner is not
discarded (`gpg --batch --yes --delete-secret-keys "$FPR"`), and run
`git-crypt lock --if-unlocked`.

### Key with a passphrase: gpg-preset-passphrase

If the deploy key has a passphrase, GPG normally asks for it through
pinentry, which fails in an unattended job (`gpg: decryption failed:
Inappropriate ioctl for device`).  You can instead pre-load the passphrase
into `gpg-agent`:

```sh
# 1. Allow presetting, then (re)start the agent so it reads the setting.
echo "allow-preset-passphrase" >> "$GNUPGHOME/gpg-agent.conf"
gpgconf --kill gpg-agent
gpgconf --launch gpg-agent

# 2. Find the keygrip of the *encryption subkey* ([E]), not the primary key.
gpg --list-secret-keys --with-keygrip

# 3. Preset the passphrase for that keygrip.
PRESET="$(gpgconf --list-dirs libexecdir)/gpg-preset-passphrase"
echo "$GPG_PASSPHRASE" | "$PRESET" --preset "$ENC_SUBKEY_KEYGRIP"

# 4. Unlock.
git-crypt unlock
```

Things to know:

- The keygrip must be that of the encryption subkey.  Presetting the primary
  key's keygrip does not help, and `git-crypt unlock` still fails.
- `gpg-agent` must be running (`gpgconf --launch gpg-agent`) and have
  `allow-preset-passphrase` set; otherwise `gpg-preset-passphrase` fails with
  `IPC connect call failed` or `Not supported`.
- The path of `gpg-preset-passphrase` varies by platform.  `gpgconf --list-dirs libexecdir` reports the directory (on the macOS Homebrew
  install used for testing it is `/opt/homebrew/Cellar/gnupg/<version>/libexec`).
- `--preset -P PASSPHRASE` also works, but puts the passphrase on the
  command line, where other local users may see it in the process list;
  prefer reading it from standard input.
- The preset is held in the agent's memory and is lost if the agent is
  restarted.  It stays cached after `git-crypt lock`; clear it with
  `gpg-connect-agent "clear_passphrase --mode=normal KEYGRIP" /bye`.

### Trade-offs

| | Symmetric key (Option 1) | GPG key without passphrase | GPG key + gpg-preset-passphrase |
|---|---|---|---|
| Secrets to store in CI | 1 (the key) | 1 (the private key) | 2 (private key and passphrase) |
| Needs `gpg` on the runner | no | yes | yes, plus agent configuration |
| Extra setup | none | key generation, `add-gpg-user`, a commit | the same, plus agent setup |
| If the CI secret leaks | all history for that key is readable | the same | the same (an attacker has the passphrase too) |

The passphrase on a deploy key adds little when both the key and the
passphrase are CI secrets that the same job can read: a leak of the job's
environment discloses both.  Choose the GPG route for its workflow (a key
per machine, listed by `git-crypt ls-gpg-users`), not as a security upgrade
over the symmetric key.  Neither approach lets you revoke access to history
that the key could already decrypt.

## Requirements and pitfalls

**A real Git checkout is required.**  `git-crypt unlock` runs Git commands
and fails in a directory that is not a Git repository, for example one
extracted from `git archive` or a source tarball:

```
fatal: not a git repository (or any of the parent directories): .git
git-crypt: Error: 'git status' failed - is this a Git repository?
```

(Files that came out of `git archive` of a locked repository are still
encrypted; there is no way to decrypt them without the clone.)  Use
`git clone` or `actions/checkout`.

**Shallow clones work.**  `actions/checkout` defaults to `fetch-depth: 1`,
which produces a shallow clone, and `git-crypt unlock` works on it:
the history is not needed, only the commit that is checked out.  Unlocking
also still works after `git fetch --unshallow`.  Git hooks, sparse
checkouts and submodules were not tested.

**The working tree must be clean.**  `git-crypt unlock` refuses to run if
there are uncommitted changes (`Error: Working directory not clean.`).
Unlock right after checkout, before the job modifies any tracked file.

### base64 portability

| Task | GNU coreutils (Linux, GitHub `ubuntu-*` runners) | macOS / BSD |
|---|---|---|
| Encode without line wrapping | `base64 -w0` | plain `base64` (no wrapping by default; `-w0` is accepted by the macOS version tested, older versions may reject it) |
| Decode | `base64 -d` (or `--decode`); wrapped input is fine | `base64 -d`, `-D` or `--decode` on the macOS version tested; older releases may only accept `-D` (not tested) |

- GNU `base64` wraps encoded output at 76 characters unless you pass `-w0`;
  macOS `base64` does not wrap by default.  `-w0` is GNU-specific and
  not guaranteed on other systems, so use plain `base64` there.  Wrapped input decodes
  correctly with `base64 -d` (checked with macOS `base64`), but a single
  line is easier to paste into a CI secret form, so use `base64 -w0` on
  Linux and plain `base64` on macOS (the key is a few hundred bytes).
- The decoding commands in the CI jobs run on the CI machine, so match the
  flag to the runner's OS: `base64 -d` on Linux runners.  `-d` works with the
  macOS `base64` tested here; older macOS releases may only accept `-D`
  (not tested).

## Locking afterwards and checking state

Lock the repository when the job is done, to remove the decrypted key from
the runner and re-encrypt the files in the working tree:

```sh
git-crypt lock --if-unlocked      # default key; succeeds if already locked
git-crypt lock --all --if-unlocked   # every key
```

`git-crypt lock` on its own fails if the repository is already locked, so
use `--if-unlocked` in cleanup steps and `trap` handlers.

To check the state in a script:

```sh
git-crypt status -r          # prints "default: unlocked" / "default: locked"
git-crypt status -r -q || git-crypt unlock -   # quiet; exit 0 only if all keys unlocked
```

`git-crypt status -r` exits 0 when every key is unlocked and 1 when any key
is locked (including a key that you deliberately never unlocked, such as
the default key when CI only has `deploy`).

## See also

- `man git-crypt` (the `unlock`, `export-key`, `lock` and `status` commands)
- [multiple_keys.md](multiple_keys.md)
