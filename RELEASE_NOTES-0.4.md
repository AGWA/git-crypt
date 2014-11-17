Changes to be aware of for git-crypt 0.4
========================================

(For a complete list of changes, see the [NEWS](NEWS.md) file.)


### New workflow

The commands for setting up a repository have changed in git-crypt 0.4.
The previous commands continue to work, but will be removed in a future
release of git-crypt.  Please get in the habit of using the new syntax:

`git-crypt init` no longer takes an argument, and is now used only when
initializing a repository for the very first time.  It generates a key
and stores it in the `.git` directory. There is no longer a separate
`keygen` step, and you no longer need to keep a copy of the key outside
the repository.

`git-crypt init` is no longer used to decrypt a cloned repository.  Instead,
run `git-crypt unlock /path/to/keyfile`, where `keyfile` is obtained by
running `git-crypt export-key /path/to/keyfile` from an already-decrypted
repository.


### GPG mode

git-crypt now supports GPG.  A repository can be shared with one or more
GPG users in lieu of sharing a secret symmetric key.  Symmetric key support
isn't going away, but the workflow of GPG mode is extremely easy and all users
are encouraged to consider it for their repositories.

See the [README](README.md) for details on using GPG.


### Status command

A new command, `git-crypt status`, lists encrypted files, which is
useful for making sure your `.gitattributes` pattern is protecting the
right files.


### Multiple key support

git-crypt now lets you encrypt different sets of files with different
keys, which is useful if you want to grant different collaborators access
to different sets of files.

See [doc/multiple_keys.md](doc/multiple_keys.md) for details.


### Compatibility with old repositories

Repositories created with older versions of git-crypt continue to work
without any changes needed, and backwards compatibility with these
repositories will be maintained indefinitely.

However, you will not be able to take advantage of git-crypt's new
features, such as GPG support, unless you migrate your repository.

To migrate your repository, first ensure the working tree is clean.
Then migrate your current key file and use the migrated key to unlock
your repository as follows:

    git-crypt migrate-key /path/to/old_key /path/to/migrated_key
    git-crypt unlock /path/to/migrated_key

Once you've confirmed that your repository is functional, you can delete
both the old and migrated key files (though keeping a backup of your key
is always a good idea).


### Known issues

It is not yet possible to revoke access from a GPG user.  This will
require substantial development work and will be a major focus of future
git-crypt development.

The output of `git-crypt status` is currently very bare-bones and will
be substantially improved in a future release.  Do not rely on its output
being stable.  A future release of git-crypt will provide an option for stable
machine-readable output.

On Windows, git-crypt does not create key files with restrictive
permissions.  Take care when using git-crypt on a multi-user Windows system.
