# GPG collaborators: workflow and troubleshooting

This page walks through giving a collaborator access to a git-crypt
repository with GPG, and how to fix the problems that most often get in the
way.  The commands and error messages below were verified against the current
development version of git-crypt and GnuPG 2.5.24.  Other GnuPG versions word some messages differently
(the troubleshooting section quotes each message exactly as that version
printed it).

You need `git-crypt init` to have been run already, and the repository to be
unlocked, on the machine where you run `add-gpg-user`.

## GPG collaborators: workflow and troubleshooting

In this walkthrough the repository owner is "you" and the collaborator is
Alice (`alice@example.com`).

### 1. Import the collaborator's public key

Ask Alice to send you her public key (never her secret key).  She exports it
with:

    gpg --armor --export alice@example.com > alice.asc

You import it into your keyring:

    gpg --import alice.asc

    gpg: key 3A8513434532FCCE: public key "Alice <alice@example.com>" imported
    gpg: Total number processed: 1
    gpg:               imported: 1

### 2. Verify the fingerprint out of band

Anyone can upload a key with any name on it.  Before you trust it, compare
its fingerprint with the one Alice reads out to you over a channel you trust
(in person, a phone call, a signed message), not the same channel that
delivered the key file:

    gpg --fingerprint alice@example.com

    pub   ed25519 2026-11-11 [SC]
          3779 EE15 43E6 E39B 1BD4  305A 3A85 1343 4532 FCCE
    uid           [ unknown] Alice <alice@example.com>
    sub   cv25519 2026-11-11 [E]
          AC9D CD8C 85E8 6B92 C00F  49BE 1569 0B5F EDCD 50C0

Alice can show hers with the same command in her own keyring.  Compare the
whole fingerprint of the primary key (the 40 hex digits), not just the last
few characters.

### 3. Add the collaborator

    git-crypt add-gpg-user --trusted alice@example.com

This encrypts the repository's git-crypt key to Alice's public key, writes it
to `.git-crypt/keys/default/0/<FINGERPRINT>.gpg`, and commits it:

    [main 7108c7f] Add 1 git-crypt collaborator
     2 files changed, 4 insertions(+)
     create mode 100644 .git-crypt/.gitattributes
     create mode 100644 .git-crypt/keys/default/0/3779EE1543E6E39B1BD4305A3A8513434532FCCE.gpg

`USER_ID` may be an email address, a key ID, or a fingerprint (see "HOW TO
SPECIFY A USER ID" in the gpg man page).

**`--trusted` or the web of trust.**  GnuPG only encrypts to a key whose
validity it considers sufficient.  A freshly imported key has validity
"unknown", so without `--trusted` GnuPG refuses it (see
[the untrusted-key error](#there-is-no-assurance-this-key-belongs-to-the-named-user)
below).  You have two ways to resolve that:

- Pass `--trusted`.  git-crypt then tells GnuPG to assume the key is valid
  (`--trust-model always`) for that encryption.  Use this only after you
  verified the fingerprint in step 2.
- Use GnuPG's web of trust.  After verifying the fingerprint, certify the
  key locally, and then `add-gpg-user` works without `--trusted`:

      gpg --quick-lsign-key 3779EE1543E6E39B1BD4305A3A8513434532FCCE

  (`--quick-lsign-key` makes a local, non-exportable certification; use
  `gpg --sign-key` if you want to publish it.)

If you identify the key by a full 40-digit fingerprint written with a `0x`
prefix, git-crypt treats it as trusted without `--trusted`, because the
fingerprint already identifies the key unambiguously:

    git-crypt add-gpg-user 0x3779EE1543E6E39B1BD4305A3A8513434532FCCE

Other options:

- `FPR!` (full 40-digit fingerprint, with or without `0x`, followed by `!`)
  encrypts to exactly that primary key or subkey, as GnuPG does.  Use it when
  a key has several encryption subkeys and you want a particular one:

      git-crypt add-gpg-user 0x<SUBKEY_FINGERPRINT>!

  git-crypt rejects `FPR!` with "cannot be used to encrypt" if that exact
  (sub)key is not an encryption key, or is expired or revoked.  Note that
  the key file is still stored under the primary key's fingerprint.
- `--force` re-encrypts the key to a user who was already added.  Without it,
  git-crypt skips existing users:

      GPG user 3779EE1543E6E39B1BD4305A3A8513434532FCCE has already been added to key 'default' - skipping.
      To re-encrypt the key to this user (e.g. after they changed encryption subkeys), use --force.

- `-k KEYNAME` selects a named key instead of `default`; `-n` / `--no-commit`
  writes the files without committing.

Add yourself too if you want to unlock fresh clones with your own GPG key
(`git-crypt init` does not do this for you):

    git-crypt add-gpg-user 0x<YOUR_FINGERPRINT>

List who can unlock the repository:

    git-crypt ls-gpg-users

    Key 'default', version 0:
        3779EE1543E6E39B1BD4305A3A8513434532FCCE Alice <alice@example.com>
        8DE0109F464B472FA28FC4026696E6FD4A74ECD9 Owner <owner@example.com>

Fingerprints that are not in your keyring are shown as
`(not in your GPG keyring)`.

### 4. Push

    git push

Everything git-crypt added is in `.git-crypt/`; the files in there are
encrypted to each collaborator's public key and are safe to publish.

### 5. The collaborator clones and unlocks

    git clone <url> project
    cd project
    git-crypt unlock

`git-crypt unlock` looks through the secret keys in the collaborator's GPG
keyring, finds the matching `.git-crypt/keys/default/0/<FINGERPRINT>.gpg`,
decrypts it (GPG asks for the key's passphrase if it has one), and decrypts
the working tree.  `git-crypt status` then shows the files as encrypted in
the repository, and they are readable in the working tree.

## Updating a collaborator's key

Two situations need a new public key from the collaborator: the key expired
(or is about to), or it never had a usable encryption subkey.  The recovery
is the same in both cases.

**Collaborator side.**  Extend the expiry of the primary key, and then of its
subkeys (the `'*'` form addresses only subkeys, so you need both commands),
and check the result:

    gpg --quick-set-expire <FINGERPRINT> 1y
    gpg --quick-set-expire <FINGERPRINT> 1y '*'
    gpg --list-keys alice@example.com

(If the problem is a missing encryption subkey, run
`gpg --quick-add-key <FINGERPRINT> default encr` instead.)  Then send the
updated public key, `gpg --armor --export alice@example.com`, to the owner.

**Owner side.**  Import the updated key; GnuPG merges the new expiry or
subkey into the copy you already have:

    gpg --import alice.asc
    gpg: key <KEYID>: "Alice <alice@example.com>" 1 new subkey

Check the fingerprint again if the key has changed hands, and then re-add:

    git-crypt add-gpg-user --force --trusted alice@example.com
    git push

(Drop `--trusted` if you have certified the key or use the `0x` fingerprint
form.)

**Do I even need to re-encrypt?**  Often not.  A collaborator whose key
expired *after* being added can still unlock existing repositories: the
`.gpg` file already in the repository stays decryptable with her secret key,
and GnuPG only prints a note:

    gpg: Note: secret key 76C87A02F377D26A expired at Tue Nov 10 22:56:33 2026 EST

You only need `add-gpg-user --force` when the owner has to produce a *new*
encrypted copy, for example when the collaborator replaced her encryption
subkey, or when she was never added because her key was already unusable.
In the latter case no `--force` is needed (nothing exists yet), just run
`add-gpg-user` again.  A re-encrypt with `--force` is also the way to
refresh the stored copy after a key change.

## Troubleshooting

Messages below are quoted from the current development version of git-crypt
with GnuPG 2.5.24.  Where a
message came from a failure that could not be reproduced here, it is marked
"not reproduced".

### GPG never asks for a passphrase and `git-crypt unlock` fails

**Symptom.**

    gpg: public key decryption failed: Inappropriate ioctl for device
    gpg: decryption failed: Inappropriate ioctl for device
    git-crypt: GPG error: Failed to decrypt

Other forms of the same problem, depending on which pinentry program is
configured:

    gpg: public key decryption failed: No pinentry
    gpg: decryption failed: No pinentry

    gpg: public key decryption failed: Operation cancelled
    gpg: decryption failed: Operation cancelled

**Cause.**  The key has a passphrase, and GnuPG could not show a prompt for
it.  `gpg` tells the agent which terminal to prompt on by reading the
`GPG_TTY` environment variable or, if that is unset, by looking at its own
standard input.  The text-mode pinentry programs (`pinentry-curses`,
`pinentry-tty`) need that information.  When `GPG_TTY` is unset and
standard input is not your terminal (typical after `su`/`sudo`, in some ssh,
IDE or script wrappers, or when input is redirected), the prompt cannot be
displayed.  `No pinentry` means the program named by
`pinentry-program` in `gpg-agent.conf` does not exist.

**Fix.**  Tell GnuPG which terminal to use, then retry:

    export GPG_TTY=$(tty)
    git-crypt unlock

Put that line in your shell's startup file (`~/.bashrc`, `~/.zshrc`) to make
it permanent.  If there is no terminal at all, or you prefer a graphical
prompt, choose a pinentry program in `~/.gnupg/gpg-agent.conf` and restart the
agent:

    pinentry-program /opt/homebrew/bin/pinentry-mac    # macOS (Homebrew)
    pinentry-program /usr/bin/pinentry-gnome3           # GNOME
    pinentry-program /usr/bin/pinentry-curses           # terminal

    gpgconf --kill gpg-agent

(Use `which pinentry-mac` etc. for the real path on your system.)  See
`man gpg-agent`, option `--pinentry-program`.

In a normal interactive terminal, GnuPG 2.5.24 found the terminal on its
own even without `GPG_TTY`; the failure above was reproduced with standard
input redirected from `/dev/null` (and with no terminal at all).  With older GnuPG versions the wording of this message
may differ.

### gpg: can't connect to the gpg-agent

**Symptom.**

    gpg: can't connect to the gpg-agent: File name too long
    git-crypt: GPG error: gpg --list-secret-keys failed

or, when the agent is not allowed to start (`no-autostart` in `gpg.conf`):

    gpg: no gpg-agent running in this session

(In this second case git-crypt continues and then fails with
"Error: no GPG secret key available to unlock this repository.", because the
keys could not be listed.)

**Cause.**  `gpg` talks to `gpg-agent` over a socket in the GnuPG home
directory (`~/.gnupg`, or `$GNUPGHOME`).  The connection fails when the
socket cannot be created or reached.  Reproduced causes: a `GNUPGHOME` whose
path is longer than the operating system's limit for socket paths (about 104
bytes on macOS, 108 on Linux), and an agent that is not running while
`no-autostart` is set.  An unwritable `GNUPGHOME` fails earlier with
`gpg: Fatal: can't create lock for '.../trustdb.gpg'` and the same
`gpg --list-secret-keys failed` message from git-crypt.

The classic "connection refused" / "IPC connect call failed" variants of
this error (a stale socket or an agent from a different GnuPG installation)
were **not reproduced**: GnuPG 2.5.24 removes a stale socket and restarts the
agent automatically.

**Fix.**  Restart the agent and check that it answers:

    gpgconf --kill gpg-agent
    gpg-connect-agent /bye

If that still fails: use a shorter `GNUPGHOME` (or let GnuPG use `/run/user/UID`
where available), make sure the directory is writable and mode 700, and remove
`no-autostart`.  `gpgconf --list-dirs agent-socket` shows the socket path that
GnuPG is trying to use.

### Unusable public key (`add-gpg-user` fails)

**Symptom.**

    gpg: 0x65BF280D2F7D8585073067DD6C24FE469D137D9D: skipped: Unusable public key
    gpg: [stdin]: encryption failed: Unusable public key
    git-crypt: GPG error: Failed to encrypt to GPG key 65BF280D2F7D8585073067DD6C24FE469D137D9D (see gpg's message above).
    This usually means GPG considers the key unusable: it may have expired or been
    revoked, or have no encryption subkey, or GPG may not trust it (see --trusted).
    Check it with 'gpg --list-keys 65BF280D2F7D8585073067DD6C24FE469D137D9D'.  If it has expired,
    import an updated copy of the public key from its owner.

No file is added to `.git-crypt` and nothing is committed.

**Cause.**  GnuPG refuses to encrypt to the key.  Three common reasons; check
which with `gpg --list-keys <user>`:

1. The key has expired (the listing shows `[expired: DATE]` on the key, and
   `[ expired]` on the user ID).
2. The key has no encryption subkey.  A key made with
   `gpg --quick-gen-key 'X <x@e>' ed25519 sign never` is signing-only, and its
   listing has no `sub ... [E]` line.
3. The key was revoked.

(The key not being trusted gives the same final line;
see [the untrusted-key error](#there-is-no-assurance-this-key-belongs-to-the-named-user).)

**Fix.**  The key's owner repairs it and sends you the updated public key.

- Expired: `gpg --quick-set-expire FPR 1y` and then
  `gpg --quick-set-expire FPR 1y '*'`.
- No encryption subkey: `gpg --quick-add-key FPR default encr`.

Then you import it and add the user again:

    gpg --import updated-key.asc
    git-crypt add-gpg-user --trusted alice@example.com

If the user was added successfully earlier and you are replacing their copy,
use `--force` (see [Updating a collaborator's key](#updating-a-collaborators-key)).
If the re-encryption fails, the existing file is left unchanged.

If you used `FPR!` and the key cannot encrypt, git-crypt stops earlier with:

    Error: GPG key 3779EE1543E6E39B1BD4305A3A8513434532FCCE cannot be used to encrypt: it is not an encryption key, or it has expired or been revoked

### There is no assurance this key belongs to the named user

**Symptom.**

    gpg: 15690B5FEDCD50C0: There is no assurance this key belongs to the named user
    gpg: [stdin]: encryption failed: Unusable public key
    git-crypt: GPG error: Failed to encrypt to GPG key 3779EE1543E6E39B1BD4305A3A8513434532FCCE (see gpg's message above).
    This usually means GPG considers the key unusable: it may have expired or been
    revoked, or have no encryption subkey, or GPG may not trust it (see --trusted).
    Check it with 'gpg --list-keys 3779EE1543E6E39B1BD4305A3A8513434532FCCE'.  If it has expired,
    import an updated copy of the public key from its owner.

The key ID in the first line is the encryption subkey's.  The line from `gpg`
is the real clue; git-crypt's own text lists trust as one possible cause.

**Cause.**  You imported the key and ran `add-gpg-user` by email address or
key ID without `--trusted`.  The key's validity is `unknown`
(`gpg --list-keys` shows `[ unknown]`), and GnuPG does not encrypt to such
keys in batch mode.

**Fix.**  After verifying the fingerprint, either re-run with `--trusted`:

    git-crypt add-gpg-user --trusted alice@example.com

or certify the key and run the command without it:

    gpg --quick-lsign-key 3779EE1543E6E39B1BD4305A3A8513434532FCCE
    git-crypt add-gpg-user alice@example.com

The `0x`-prefixed full fingerprint form is always treated as trusted.

### Error: no GPG secret key available to unlock this repository

**Symptom.**

    Error: no GPG secret key available to unlock this repository.
    To unlock with a shared symmetric key instead, specify the path to the symmetric key as an argument to 'git-crypt unlock'.
    To see a list of GPG keys authorized to unlock this repository, run 'git-crypt ls-gpg-users'.

**Cause.**  None of the secret keys that git-crypt found in the keyring has a
matching `.git-crypt/keys/<KEYNAME>/<VERSION>/<FINGERPRINT>.gpg` file in the
repository.  Usual reasons:

- You are using a different keyring than the one holding your key: a different
  user account, another machine, or a `GNUPGHOME` set in your environment.
  A first-time `gpg` run in an empty home also prints
  `gpg: keybox '.../pubring.kbx' created`.
- The secret key was never imported on this machine.
- You were never added: your key has a secret key but the owner did not run
  `add-gpg-user` for it.  Compare `git-crypt ls-gpg-users` with
  `gpg --list-secret-keys --fingerprint`.
- `gpg` and `gpg2` are different programs on your system and use different
  keyrings or settings (for example GnuPG 1.x installed as `gpg` and 2.x as
  `gpg2`).  This scenario itself was not reproduced; only the `gpg.program`
  lookup was.

**Fix.**

- List what git-crypt can see: `gpg --list-secret-keys --fingerprint` and
  `git-crypt ls-gpg-users`.  A fingerprint from the second command must
  appear in the first.
- Import the key from your backup:
  `gpg --import my-secret-key.asc` (or copy your `~/.gnupg` directory).
- Point git-crypt at the right executable: git-crypt runs the program named in
  the Git setting `gpg.program` (default: `gpg`), the same setting Git uses for
  signing.

      git config gpg.program gpg2          # this repository
      git config --global gpg.program gpg2

- If you were never added, ask the owner to run
  `git-crypt add-gpg-user <your key>`, push, and then `git pull` and unlock.

If the program in `gpg.program` does not exist, you get this instead:

    /nonexistent/gpg2: No such file or directory
    git-crypt: GPG error: gpg --list-secret-keys failed

### An expired collaborator key

See [Updating a collaborator's key](#updating-a-collaborators-key).  In short:
the collaborator extends the expiry of the key and its subkeys and sends you
the updated public key; you `gpg --import` it and run
`git-crypt add-gpg-user --force FPR`; commit and push (`add-gpg-user`
commits for you).  Existing collaborators keep working with the old file; you
only need the re-encrypt when you must (re)create their copy.
