#!/usr/bin/env bash
#
# Runs git-crypt's integration scenarios against the built binary.
#
# Usage: tests/run.sh [tests/tNNNN-name.sh ...]
#
# With no arguments, every tests/t[0-9]*.sh file is run.  Each file defines
# shell functions named scenario_* (expected to pass) or xfail_* (known
# failures, reported but not fatal).  Every scenario runs in its own
# subshell, in a fresh temporary directory, with an isolated HOME, Git
# configuration and GNUPGHOME.
#
# Exit status: 0 if every scenario passed (or was skipped / an expected
# failure), 1 otherwise.

set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)

GIT_CRYPT_BIN=${GIT_CRYPT_BIN:-$root/git-crypt}
if [ ! -x "$GIT_CRYPT_BIN" ] && [ -x "$GIT_CRYPT_BIN.exe" ]; then
	GIT_CRYPT_BIN=$GIT_CRYPT_BIN.exe
fi
if [ ! -x "$GIT_CRYPT_BIN" ]; then
	echo "git-crypt binary not found at $GIT_CRYPT_BIN - run 'make' first" >&2
	exit 2
fi
bin_dir=$(cd "$(dirname "$GIT_CRYPT_BIN")" && pwd)

if [ $# -gt 0 ]; then
	files=("$@")
else
	files=("$here"/t[0-9]*.sh)
fi

passed=0
failed=0
skipped=0
xfailed=0
xpassed=0
failures=()

run_scenario ()
{
	# run_scenario FILE FUNCTION TMPDIR GNUPGHOME
	local file=$1 fn=$2 tmp=$3 gnupghome=$4
	(
		export SCENARIO_TMP=$tmp
		export HOME=$tmp/home
		export GNUPGHOME=$gnupghome
		# On Windows (MSYS2), don't convert GNUPGHOME to a Windows path when
		# starting git-crypt.exe: the gpg it runs is MSYS2's, which treats a
		# Windows path as relative.  No effect on other platforms.
		export MSYS2_ENV_CONV_EXCL="GNUPGHOME${MSYS2_ENV_CONV_EXCL:+;$MSYS2_ENV_CONV_EXCL}"
		export GIT_CONFIG_GLOBAL=/dev/null
		export GIT_CONFIG_NOSYSTEM=1
		export GIT_AUTHOR_NAME='git-crypt test'
		export GIT_AUTHOR_EMAIL='test@example.com'
		export GIT_COMMITTER_NAME='git-crypt test'
		export GIT_COMMITTER_EMAIL='test@example.com'
		export GIT_TERMINAL_PROMPT=0
		export PATH="$bin_dir:$PATH"
		unset GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE GIT_CEILING_DIRECTORIES
		mkdir -p "$HOME"
		cd "$tmp"
		. "$here/lib.sh"
		. "$file"
		set -e
		"$fn"
	)
}

for file in "${files[@]}"; do
	file=$(cd "$(dirname "$file")" && pwd)/$(basename "$file")
	scenarios=$(bash -c '. "$1"; . "$2"; declare -F' _ "$here/lib.sh" "$file" |
		awk '{ print $3 }' | grep -E '^(scenario|xfail)_' || true)

	for fn in $scenarios; do
		name="$(basename "$file" .sh): $fn"
		tmp=$(mktemp -d "${TMPDIR:-/tmp}/git-crypt-test.XXXXXX")
		# gpg-agent's socket path must be short, so don't use $TMPDIR (long on macOS).
		gnupghome=$(mktemp -d /tmp/gcg.XXXXXX)
		chmod 700 "$gnupghome"

		run_scenario "$file" "$fn" "$tmp" "$gnupghome" >"$tmp.log" 2>&1
		rc=$?

		GNUPGHOME=$gnupghome gpgconf --kill all >/dev/null 2>&1 || true
		extra_gnupghomes=
		if [ -f "$tmp/extra-gnupghomes" ]; then
			extra_gnupghomes=$(cat "$tmp/extra-gnupghomes")
			for home in $extra_gnupghomes; do
				GNUPGHOME=$home gpgconf --kill all >/dev/null 2>&1 || true
			done
		fi

		case "$fn:$rc" in
			scenario_*:0)
				echo "ok      $name"
				passed=$((passed + 1)) ;;
			*:77)
				echo "skip    $name ($(grep '^SKIP:' "$tmp.log" | head -n 1))"
				skipped=$((skipped + 1)) ;;
			xfail_*:0)
				echo "XPASS   $name (expected failure passed - consider promoting it to scenario_)"
				xpassed=$((xpassed + 1)) ;;
			xfail_*:*)
				echo "xfail   $name"
				xfailed=$((xfailed + 1)) ;;
			*)
				echo "FAILED  $name (exit $rc)"
				sed 's/^/    | /' "$tmp.log" | tail -n 30
				failed=$((failed + 1))
				failures+=("$name") ;;
		esac

		rm -rf "$tmp" "$tmp.log" "$gnupghome" $extra_gnupghomes
	done
done

echo
echo "passed: $passed, failed: $failed, skipped: $skipped, expected failures: $xfailed, unexpected passes: $xpassed"
if [ "$failed" -ne 0 ]; then
	echo "Failed scenarios:"
	for name in "${failures[@]}"; do
		echo "    $name"
	done
	exit 1
fi
exit 0
