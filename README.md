git-crypt - transparent file encryption in git
==============================================

git-crypt enables transparent encryption and decryption of files in a
git repository.  Files which you choose to protect are encrypted when
committed, and decrypted when checked out.  git-crypt lets you freely
share a repository containing a mix of public and private content.
git-crypt gracefully degrades, so developers without the secret key can
still clone and commit to a repository with encrypted files.  This lets
you store your secret material (such as keys or passwords) in the same
repository as your code, without requiring you to lock down your entire
repository.

git-crypt was written by [Andrew Ayer](https://www.agwa.name) (agwa@andrewayer.name).
For more information, see <https://www.agwa.name/projects/git-crypt>.

Building git-crypt
------------------
See the [INSTALL.md](INSTALL.md) file.

To run the integration tests, run `make test` (see
[tests/README.md](tests/README.md)).


Using git-crypt
---------------

Configure a repository to use git-crypt:

    cd repo
    git-crypt init

Specify files to encrypt by creating a .gitattributes file:

    secretfile filter=git-crypt diff=git-crypt
    *.key filter=git-crypt diff=git-crypt
    secretdir/** filter=git-crypt diff=git-crypt

Like a .gitignore file, it can match wildcards and should be checked into
the repository.  See below for more information about .gitattributes.
Make sure you don't accidentally encrypt the .gitattributes file itself
(or other git files like .gitignore or .gitmodules).  Make sure your
.gitattributes rules are in place *before* you add sensitive files, or
those files won't be encrypted!

Share the repository with others (or with yourself) using GPG:

    git-crypt add-gpg-user USER_ID

`USER_ID` can be a key ID, a full fingerprint, an email address, or
anything else that uniquely identifies a public key to GPG (see "HOW TO
SPECIFY A USER ID" in the gpg man page).  Note: `git-crypt add-gpg-user`
will add and commit a GPG-encrypted key file in the .git-crypt directory
of the root of your repository.

To see which GPG keys can unlock the repository:

    git-crypt ls-gpg-users

Alternatively, you can export a symmetric secret key, which you must
securely convey to collaborators (GPG is not required, and no files
are added to your repository):

    git-crypt export-key /path/to/key

After cloning a repository with encrypted files, unlock with GPG:

    git-crypt unlock

Or with a symmetric key:

    git-crypt unlock /path/to/key

That's all you need to do - after git-crypt is set up (either with
`git-crypt init` or `git-crypt unlock`), you can use git normally -
encryption and decryption happen transparently.

To lock the repository again (remove the decrypted key and re-encrypt the
files in the working tree):

    git-crypt lock

`git-crypt lock` fails if the repository is already locked; use `git-crypt
lock --if-unlocked` in cleanup scripts that should succeed either way.

To check whether the repository is unlocked (for example from a script or
a shell prompt), use `git-crypt status -r`.  It prints the state of each
key and exits with status 0 if every key is unlocked, or 1 if any is
locked; add `-q` to suppress the output:

    git-crypt status -r -q || git-crypt unlock

For unlocking in CI and deployment (including a GitHub Actions example and
the security cautions that go with it), see [doc/ci.md](doc/ci.md).

If the repository has multiple worktrees (`git worktree add`), they all
share the same keys: `git-crypt unlock` and `git-crypt lock` decrypt or
encrypt the files in every worktree at once.

Where keys are stored
---------------------

**Unlocked key.**  `git-crypt init` and `git-crypt unlock` keep the
symmetric key in the Git directory, never in the work tree:

    $(git rev-parse --git-common-dir)/git-crypt/keys/<name>

where `<name>` is `default` for the default key, or the name given with
`-k`.  This file is never committed.  Because it lives in the common Git
directory, all linked worktrees (`git worktree add`) share it.
`git-crypt lock` removes it.

**Committed GPG copies.**  `git-crypt add-gpg-user` commits a copy of the
key, encrypted to the collaborator's GPG key, in the work tree:

    .git-crypt/keys/<name>/<version>/<FPR>.gpg

There is one file per collaborator, named by the full fingerprint of
their GPG key.  `git-crypt ls-gpg-users` lists them.  `git-crypt unlock`
(with no argument) decrypts one of these using your GPG secret key and
stores the result as the unlocked key above.

**Exported keys.**  `git-crypt export-key` writes the raw, unencrypted
symmetric key to the file you name.  Anyone with that file can decrypt
the repository, so protect it like a password.

Moving the committed keys: git-crypt.repoStateDir
-------------------------------------------------

By default, git-crypt looks for the committed keys in `.git-crypt` at
the top of the work tree.  If they live somewhere else, set the
`git-crypt.repoStateDir` option to that directory, relative to the top
of the work tree:

    git config git-crypt.repoStateDir path/to/.git-crypt

This is local Git configuration and is not committed, so every clone
must set it.  It only changes where the committed GPG copies are read
and written; it does not move the unlocked key, which always stays in
the Git directory.  Absolute paths are not supported: the value is
always appended to the top of the work tree, so an absolute path will
not be found and `git-crypt unlock` will fail.

A typical use is a project that has its own `.git-crypt` directory and
is added to another repository with `git subtree`:

    git subtree add --prefix=sub /path/to/sub-repo main
    git config git-crypt.repoStateDir sub/.git-crypt
    git-crypt unlock

Without the `git config` line, git-crypt looks for `.git-crypt` at the
top of the outer repository, finds no keys, and `git-crypt unlock` fails
with "no GPG secret key available to unlock this repository".  With it,
unlock finds `sub/.git-crypt/keys/default/0/<FPR>.gpg` and the files
under `sub/` are decrypted.

Verifying encryption
--------------------

To check that your files are really stored encrypted, run `git-crypt
status`.  It lists every file in the repository, tracked or not, and says
whether git-crypt will encrypt it according to `.gitattributes`:

    $ git-crypt status
    not encrypted: .gitattributes
    not encrypted: README
        encrypted: secrets/api.txt

`git-crypt status -e` shows only the encrypted files, and `git-crypt
status -u` shows only the unencrypted ones.  You can also give it file
names to check just those files.  (`-z`, for machine-readable output, is
not implemented yet.)

For files that are supposed to be encrypted, `status` also looks at what
Git actually has in the index and in HEAD, and warns if it is plaintext:

    $ git-crypt status
    not encrypted: .gitattributes
    not encrypted: README
        encrypted: early.txt *** WARNING: staged/committed version is NOT ENCRYPTED! ***
        encrypted: secrets/api.txt

    Warning: one or more files is marked for encryption via .gitattributes but
    was staged and/or committed before the .gitattributes file was in effect.
    Run 'git-crypt status' with the '-f' option to stage an encrypted version.

This typically happens when a file was added before git-crypt was set up
or before the file was covered by `.gitattributes`.  `git-crypt status -f`
stages an encrypted version of each such file (you still need to commit):

    $ git-crypt status -f
    early.txt: staged encrypted version
    Staged 1 encrypted file.
    Warning: if these files were previously committed, unencrypted versions still exist in the repository's history.

`-f` can only encrypt a file while the repository is unlocked.  If it is
locked, `git-crypt status -f` refuses and tells you to run `git-crypt
unlock` first.  (If `unlock` complains that the working directory is not
clean, `git stash` your changes before unlocking and `git stash pop` them
afterwards.)

A second warning, "diff=git-crypt attribute not set", means a file has
`filter=git-crypt` but not `diff=git-crypt` in `.gitattributes`.  The
file is still encrypted when committed, but `git diff` will not be able
to show readable changes; add `diff=git-crypt` to the pattern.

`git-crypt status` exits with status 1 if it printed either warning.

### Checking without git-crypt

Encrypted blobs begin with the 10 bytes `\0GITCRYPT\0`.  You can check
what Git has stored, without relying on git-crypt at all, by reading the
blob directly.  For the committed version of a file:

    $ git cat-file -p HEAD:secrets/api.txt | head -c 10 | xxd
    00000000: 0047 4954 4352 5950 5400                 .GITCRYPT.

(`od -c` works too and shows the NUL bytes as `\0`.)  For the version in
the index, i.e. what you have staged and is about to be committed, use
`:path` instead of `HEAD:path`:

    $ git cat-file -p :secrets/api.txt | head -c 10 | xxd
    00000000: 0047 4954 4352 5950 5400                 .GITCRYPT.

A plaintext file shows its own contents instead:

    $ git cat-file -p HEAD:README | head -c 10 | xxd
    00000000: 6865 6c6c 6f0a                           hello.

### Do not use git diff or git show to check

When the repository is unlocked, `git diff` and `git show` display files
as plaintext, so they cannot tell you whether a file is encrypted.  The
`diff=git-crypt` attribute makes Git run a "textconv" filter that
decrypts the stored blob before comparing it:

    $ git show HEAD -- secrets/api.txt
    ...
    diff --git a/secrets/api.txt b/secrets/api.txt
    index 5150825..9df0775 100644
    --- a/secrets/api.txt
    +++ b/secrets/api.txt
    @@ -1 +1 @@
    -api key: hunter2
    +api key: new

Pass `--no-textconv` to see the stored (encrypted) data as Git sees it:

    $ git show --no-textconv HEAD -- secrets/api.txt
    ...
    diff --git a/secrets/api.txt b/secrets/api.txt
    index 5150825..9df0775 100644
    Binary files a/secrets/api.txt and b/secrets/api.txt differ

When the repository is locked, `git diff`, `git diff --cached` and `git
show` print "Binary files ... differ" for these files whether or not
they are encrypted: a plaintext file staged while the repository was
locked looks exactly the same.  Use `git-crypt status` or `git cat-file`
as shown above.

### History

`git-crypt status` checks the files as they are now, in the index and in
HEAD.  It does not look at older commits.  If a file was committed
unencrypted at some point, fixing it with `status -f` only protects new
commits; the plaintext is still in the history, and anyone with a copy of
the repository can read it:

    $ git cat-file -p HEAD~2:early.txt
    secret before attrs

Removing it requires rewriting history (and rotating any secrets that
were exposed).  A separate document on rewriting history is planned.

Current Status
--------------

The latest version of git-crypt is [0.8.0](NEWS.md), released on
2025-09-23.  git-crypt aims to be bug-free and reliable, meaning it
shouldn't crash, malfunction, or expose your confidential data.
However, it has not yet reached maturity, meaning it is not as
documented, featureful, or easy-to-use as it should be.  Additionally,
there may be backwards-incompatible changes introduced before version
1.0.

Security
--------

git-crypt is more secure than other transparent git encryption systems.
git-crypt encrypts files using AES-256 in CTR mode with a synthetic IV
derived from the SHA-1 HMAC of the file.  This mode of operation is
provably semantically secure under deterministic chosen-plaintext attack.
That means that although the encryption is deterministic (which is
required so git can distinguish when a file has and hasn't changed),
it leaks no information beyond whether two files are identical or not.
Other proposals for transparent git encryption use ECB or CBC with a
fixed IV.  These systems are not semantically secure and leak information.

For the encrypted file format, the crypto design, the key file format, and
recipes for decrypting a blob or a collaborator's `.gpg` key file by hand,
see [doc/how-it-works.md](doc/how-it-works.md).

Limitations
-----------

git-crypt relies on git filters, which were not designed with encryption
in mind.  As such, git-crypt is not the best tool for encrypting most or
all of the files in a repository. Where git-crypt really shines is where
most of your repository is public, but you have a few files (perhaps
private keys named *.key, or a file with API credentials) which you
need to encrypt.  For encrypting an entire repository, consider using a
system like [git-remote-gcrypt](https://spwhitton.name/tech/code/git-remote-gcrypt/)
instead.  (Note: no endorsement is made of git-remote-gcrypt's security.)

git-crypt does not encrypt file names, commit messages, symlink targets,
gitlinks, or other metadata.

git-crypt does not hide when a file does or doesn't change, the length
of a file, or the fact that two files are identical (see "Security"
section above).

git-crypt does not support revoking access to an encrypted repository
which was previously granted. This applies to both multi-user GPG
mode (there's no del-gpg-user command to complement add-gpg-user)
and also symmetric key mode (there's no support for rotating the key).
This is because it is an inherently complex problem in the context
of historical data. For example, even if a key was rotated at one
point in history, a user having the previous key can still access
previous repository history. This problem is discussed in more detail in
<https://github.com/AGWA/git-crypt/issues/47>.

Files encrypted with git-crypt are not compressible.  Even the smallest
change to an encrypted file requires git to store the entire changed file,
instead of just a delta.

Although git-crypt protects individual file contents with a SHA-1
HMAC, git-crypt cannot be used securely unless the entire repository is
protected against tampering (an attacker who can mutate your repository
can alter your .gitattributes file to disable encryption).  If necessary,
use git features such as signed tags instead of relying solely on
git-crypt for integrity.

Files encrypted with git-crypt cannot be patched with git-apply, unless
the patch itself is encrypted.  To generate an encrypted patch, use `git
diff --no-textconv --binary`.  Alternatively, you can apply a plaintext
patch outside of git using the patch command.

git-crypt does not work reliably with some third-party git GUIs, such
as [Atlassian SourceTree](https://jira.atlassian.com/browse/SRCTREE-2511)
and GitHub for Mac.  Files might be left in an unencrypted state.

Gitattributes File
------------------

The .gitattributes file is documented in the gitattributes(5) man page.
The file pattern format is the same as the one used by .gitignore,
as documented in the gitignore(5) man page, with the exception that
specifying merely a directory (e.g. `/dir/`) is *not* sufficient to
encrypt all files beneath it.

Also note that the pattern `dir/*` does not match files under
sub-directories of dir/.  To encrypt an entire sub-tree dir/, use `dir/**`:

    dir/** filter=git-crypt diff=git-crypt

The .gitattributes file must not be encrypted, so make sure wildcards don't
match it accidentally.  If necessary, you can exclude .gitattributes from
encryption like this:

    .gitattributes !filter !diff
