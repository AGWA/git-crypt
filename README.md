# git-crypt - a transparent file encryption in git

<!--- Logo picture element for user's light/dark modes --->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="./media/git-crypt-logo-dark.svg">
  <img alt="git-crypt logo header" src="./media/git-crypt-logo.svg">
</picture>

**git-crypt** enables transparent encryption and decryption of files in your git repository.

Simply select all the files that you want to protect. From that point forward, all your selected files will be encrypted when they are committed and decrypted when they are checked out.

With **git-crypt** you can freely share your repository with the public while keeping your private or sensitive content secure.

**git-crypt** also gracefully degrades, so contributors and developers can still clone and commit changes to your repository while the encrypted files remain secure. Your secret material (such as keys or passwords) can be kept in the same repository as your code, without requiring you to lock down your entire repository.

**git-crypt** was written by [Andrew Ayer](https://www.agwa.name) (agwa@andrewayer.name).

For more information, see <https://www.agwa.name/projects/git-crypt>.

<!--- Line break elements have been added to improve readability -->
<br />

## Installing git-crypt

1. For ***nix** based systems.

        apt-get install git-crypt

2. For **MacOS** (using homebrew).

        brew install git-crypt

3. To build and install from source.

    > Following the instructions in the [INSTALL.md](INSTALL.md) file.

<br />

## Setting up git-crypt

1. Start by configuring your repository to use git-crypt.

        cd your-repo/
        git-crypt init

    This will generate a key for your repository.


2. Specify the files you want to encrypt by creating a `.gitattributes` file.

    For example, let's say you have a file called `secretfile` and maybe a directory called `secretdir/`. You can add them like this:

    ```
    # My secret file
    secretfile filter=git-crypt diff=git-crypt

    # My secret directory
    secretdir/** filter=git-crypt diff=git-crypt

    # You can even add a key
    # My secret key
    *.key filter=git-crypt diff=git-crypt
    ```

    You can use [globbing patterns](http://linux.die.net/man/7/glob) to match against your file names, just like in your .gitignore file. ([See below](#gitattributes-file) for more information about .gitattributes.)

    > || **WARNING** ||
    >
    > * Make sure your .gitattributes rules are in place **BEFORE** you commit sensitive files, or those files won't be encrypted!
    >
    > * Be care not to (accidently) encrypt the .gitattributes file itself
    > (or other git files like .gitignore or .gitmodules).

<br />

## Encrypting your files

You can either choose to encrypt your files automatically using git-crypt, or you can choose to encrypt and decrypt manually at any time.

1. **Automatic** Encryption/Decryption.

    * When you *commit* your repo, your files will be automatically **encrypted**.

    * When you *checkout* your repository, your files will be automatically **decrypted**.

2. Alternatively, you can **manually** encrypt and decrypt your files.

    * Lock selected files in your repository.

        ```
        git-crypt lock
        ```

    * Unlock selected files in your reposity.

        ```
        git-crypt unlock
        ```

<br />

## Collaborating with Others

In order for others to en/decrypt your files, they will need a public key. You can generate your key either by using [GPG](https://gnupg.org/download/), or by using **git-crypt**.

1. Create and commit a GPG user using [GPG](https://gnupg.org/download/).

    ```
    git-crypt add-gpg-user USER_ID
    ```


    > `USER_ID` can be: a key ID a full fingerprint, an email address, or anything else that uniquely identifies a public key to GPG.
    >
    > (see ["HOW TO SPECIFY A USER ID"](https://www.gnupg.org/documentation/manuals/gnupg/Specify-a-User-ID.html))

    This will create a `.git-crypt/` directory in the root folder of your repository and add (and commit) a GPG-encrypted key file for each user you create.

2. Create/Send a sharable key using git-crypt.

    You can also export a symmetric secret key, which you will need to
securely convey to collaborators (GPG is not required, and no files
are added to your repository).

        git-crypt export-key /path/to/key

    You can then send this key to your collaborators, who can unlock your encrypted files using:

        git-crypt unlock /path/to/key

<br />

## Using git-crypt

Once **git-crypt** is set up (either with
`git-crypt init` or `git-crypt unlock`), you can continue to use git normally. Encryption and decryption will happen automatically and transparently.

<br />

## More About Development

### Current Status

The latest version of git-crypt is [0.7.0](NEWS.md), released on
2022-04-21.  git-crypt aims to be bug-free and reliable, meaning it
shouldn't crash, malfunction, or expose your confidential data.
However, it has not yet reached maturity, meaning it is not as
documented, featureful, or easy-to-use as it should be.  Additionally,
there may be backwards-incompatible changes introduced before version
1.0.

### Security

git-crypt is more secure than other transparent git encryption systems.
git-crypt encrypts files using AES-256 in CTR mode with a synthetic IV
derived from the SHA-1 HMAC of the file.  This mode of operation is
provably semantically secure under deterministic chosen-plaintext attack.
That means that although the encryption is deterministic (which is
required so git can distinguish when a file has and hasn't changed),
it leaks no information beyond whether two files are identical or not.
Other proposals for transparent git encryption use ECB or CBC with a
fixed IV.  These systems are not semantically secure and leak information.

### Limitations

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


### Gitattributes File

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
