# git-crypt integration tests

The tests are black-box scenarios: shell functions that drive the built
`git-crypt` binary together with `git` and `gpg`.  No C++ test hooks are
involved.

## Running

    make test                       # build, then run every scenario
    tests/run.sh                    # run every scenario against ./git-crypt
    tests/run.sh tests/t0001-basic.sh   # run one file

Set `GIT_CRYPT_BIN` to test a binary somewhere else.  `bash`, `git` (2.32 or
newer, for `GIT_CONFIG_GLOBAL`) and, for the GPG scenarios, `gpg` must be
installed.  Scenarios that need `gpg` are skipped if it is missing.

## Isolation

Every scenario runs in its own subshell and temporary directory, with:

* `HOME` pointing into the temporary directory;
* `GIT_CONFIG_GLOBAL=/dev/null` and `GIT_CONFIG_NOSYSTEM=1`, so your
  `~/.gitconfig` (for example `commit.gpgsign`) cannot leak in;
* a fresh `GNUPGHOME` under `/tmp` (gpg-agent's socket path must be short,
  which `$TMPDIR` is not on macOS).

The temporary directories are removed and any gpg-agent is stopped after
each scenario, whether it passed or not.

## Writing scenarios

Add functions to a `tests/tNNNN-topic.sh` file:

* `scenario_name ()` must succeed.  The body runs under `set -e`, so any
  failing command fails the scenario.
* `xfail_name ()` is a known failure.  It is reported as `xfail` and doesn't
  fail the suite; if it starts passing it is reported as `XPASS`, as a
  reminder to rename it to `scenario_`.

Helpers are in `tests/lib.sh`: `new_repo`, `new_encrypted_repo` (encrypts
`secret*`), `run` (captures `$status`, `$stdout`, `$stderr`), `assert_eq`,
`assert_status`, `assert_contains`, `assert_blob_encrypted`,
`assert_file_encrypted`, `assert_file_content`, `gpg_gen_key` and `skip`.
