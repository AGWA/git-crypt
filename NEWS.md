News
====

######v0.3 (2013-04-05)
*   Fix `git-crypt init` on newer versions of Git.  Previously,
    encrypted files were not being automatically decrypted after running
    `git-crypt init` with recent versions of Git.
*   Allow `git-crypt init` to be run even if the working tree contains
    untracked files.
*   `git-crypt init` now properly escapes arguments to the filter
    commands it configures, allowing both the path to git-crypt and the
    path to the key file to contain arbitrary characters such as spaces.

######v0.2 (2013-01-25)
*   Numerous improvements to `git-crypt init` usability.
*   Fix gitattributes example in [README](README.md): the old example
    showed a colon after the filename where there shouldn't be one.
*   Various build fixes and improvements.

######v0.1 (2012-11-29)
*   Initial release.
