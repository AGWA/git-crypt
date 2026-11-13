Thanks for your interest in contributing to git-crypt!  git-crypt is
open source software and welcomes contributions in the form of code,
documentation, bug reports, or anything else that improves git-crypt.

When contributing code, please consider the following guidelines:

 * You are encouraged to open an issue on GitHub to discuss any non-trivial
   changes before you start coding.

 * Please mimic the existing code style as much as possible.  In
   particular, please indent code using tab characters with a width
   of 8.

 * Please add or update an integration scenario under `tests/` for your
   change, and make sure `make test` passes (see tests/README.md).

 * To minimize merge commits, please rebase your changes before opening
   a pull request.

 * To submit your patch, open a pull request on GitHub.

Finally, be aware that since git-crypt is security-sensitive software,
the bar for contributions is higher than average.  Please don't be
discouraged by this, but be prepared for patches to possibly go through
several rounds of feedback and improvement before being accepted.
Your patience and understanding is appreciated.

Making a release
----------------

Maintainers make a release from GitHub Actions:

 1. Make sure the `## Unreleased` section of NEWS.md lists the changes.

 2. Run the Prepare release workflow, choosing which part of the version
    to increment (e.g. 0.8.0 becomes 0.8.1, 0.9.0 or 1.0.0):

        gh workflow run prepare-release.yml -f bump=minor

    It opens a "Release 0.9.0" pull request that updates the version in
    `git-crypt.hpp`, the man page and README.md, and renames
    `## Unreleased` in NEWS.md to `## v0.9.0 (date)`.

 3. Review the pull request (edit the NEWS.md section there if needed)
    and merge it.

Merging it runs the Release workflow, which tags the merge commit `0.9.0`,
creates a draft release with that NEWS.md section as its notes, builds the
Linux, macOS and Windows binaries, attaches them with a SHA256SUMS file,
and publishes the release once every binary is attached.  If a build
fails, the release stays a draft; finish it with
`gh workflow run release.yml -f tag=0.9.0`.
