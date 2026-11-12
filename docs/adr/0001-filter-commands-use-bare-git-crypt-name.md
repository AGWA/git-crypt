# Filter commands run git-crypt by its bare name

Status: accepted (issue #22)

## Context

`git-crypt init` and `git-crypt unlock` write the smudge, clean and textconv
commands into `.git/config`.  They used to write the path git-crypt was
invoked by: an absolute path if it was run by one, the bare name `git-crypt`
if it was found via `PATH` (and, on Windows, always the absolute path of the
executable).  An absolute path breaks the repository when git-crypt is
upgraded or moved.  The bare name breaks it for GUI clients that don't
inherit the shell's `PATH`, and is resolved via `PATH` each time Git runs
a filter.

## Decision

- The filter commands run the bare name `git-crypt` by default, however
  git-crypt was invoked (as AGWA/git-crypt#217 requests), on every platform.
- The `git-crypt.program` git config option overrides it.  It is read with
  the standard `git config` lookup (system, global, local) when the filters
  are written, and its value is copied into the commands.  Empty values and
  relative paths containing `/` are rejected; anything else (an absolute
  path or a bare command name) is used as is.
- `git-crypt refresh-filters` rewrites the commands of every key whose
  filter is configured.  It needs no key and no clean work tree (so it works
  even when the configured binary is gone, which makes `git status` fail),
  takes no options, and does nothing in a locked repository.  `init`,
  `unlock` and `refresh-filters` print an `old -> new` notice for each key
  whose commands changed, plus a hint about `git-crypt.program` when a path
  is replaced by a bare name.
- git-crypt doesn't detect stale commands by itself.

## Security

A malicious directory on `PATH` could run its own `git-crypt` and see
plaintext.  We don't treat this as a new risk: the same directory could
shadow `git`, `gpg` or the shell, which see the same data.  Pinning a path
with `git-crypt.program` is an explicit opt-in for users who want it.

## Consequences

- Repositories keep working across upgrades and moves without action.
- GUI clients without the shell's `PATH` need a one-time
  `git config --global git-crypt.program /path/to/git-crypt` followed by
  `git-crypt refresh-filters` in each unlocked repository.
- Repositories configured with an absolute path by an older git-crypt keep
  it until `git-crypt refresh-filters` (or another `unlock`) runs.
