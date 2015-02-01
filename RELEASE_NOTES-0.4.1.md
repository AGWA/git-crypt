git-crypt 0.4.1 is a bugfix-only release that contains an important
usability fix for users who use GPG mode to encrypt an entire repository.

Previously, if you used a '*' pattern in the top-level .gitattributes
file, and you did not explicitly add a pattern to exclude the .git-crypt
directory, the files contained therein would be encrypted, rendering
the repository impossible to unlock with GPG.

git-crypt now adds a .gitattributes file to the .git-crypt directory
to prevent its contents from being encrypted, regardless of patterns in
the top-level .gitattributes.

If you are using git-crypt in GPG mode to encrypt an entire repository,
and you do not already have a .gitattributes pattern to exclude the
.git-crypt directory, you are strongly advised to upgrade.  After
upgrading, you should do the following in each of your repositories to
ensure that the information inside .git-crypt is properly stored:

1. Remove existing key files: `rm .git-crypt/keys/*/0/*`

2. Re-add GPG user(s): `git-crypt add-gpg-user GPG_USER_ID ...`
