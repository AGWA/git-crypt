In addition to the implicit default key, git-crypt supports alternative
keys which can be used to encrypt specific files and can be shared with
specific GPG users.  This is useful if you want to grant different
collaborators access to different sets of files.

To generate an alternative key named KEYNAME, pass the `-k KEYNAME`
option to `git-crypt init` as follows:

    git-crypt init -k KEYNAME

To encrypt a file with an alternative key, use the `git-crypt-KEYNAME`
filter in `.gitattributes` as follows:

    secretfile filter=git-crypt-KEYNAME diff=git-crypt-KEYNAME

To export an alternative key or share it with a GPG user, pass the `-k
KEYNAME` option to `git-crypt export-key` or `git-crypt add-gpg-user`
as follows:

    git-crypt export-key -k KEYNAME /path/to/keyfile
    git-crypt add-gpg-user -k KEYNAME GPG_USER_ID

To unlock a repository with an alternative key, use `git-crypt unlock`
normally.  git-crypt will automatically determine which key is being used.
