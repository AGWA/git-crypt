News
====

######v0.4 (2014-11-16)
(See [the release notes](RELEASE_NOTES-0.4.md) for important details.)
*   Add optional GPG support: GPG can be used to share the repository
    between one or more users in lieu of sharing a secret key.
*   New workflow: the symmetric key is now stored inside the .git
    directory.  Although backwards compatibility has been preserved
    with repositories created by old versions of git-crypt, the
    commands for setting up a repository have changed.  See the
    release notes file for details.
*   Multiple key support: it's now possible to encrypt different parts
    of a repository with different keys.
*   Initial `git-crypt status` command to report which files are
    encrypted and to fix problems that are detected.
*   Numerous usability, documentation, and error reporting improvements.
*   Major internal code improvements that will make future development
    easier.
*   Initial experimental Windows support.

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
