# Helpers for git-crypt integration scenarios.  Sourced by tests/run.sh
# inside a fresh subshell for every scenario; see tests/README.md.

GIT_CRYPT_HEADER_HEX=00474954435259505400 # "\0GITCRYPT\0" as hex (10 bytes)

fail ()
{
	echo "FAIL: $*" >&2
	exit 1
}

# Skip the current scenario (for example when gpg is not installed).
skip ()
{
	echo "SKIP: $*" >&2
	exit 77
}

# run CMD ARGS...: run a command without aborting the scenario, capturing
# its exit status in $status, and its output in $stdout and $stderr.
run ()
{
	set +e
	"$@" >"$SCENARIO_TMP/run.stdout" 2>"$SCENARIO_TMP/run.stderr"
	status=$?
	set -e
	stdout=$(cat "$SCENARIO_TMP/run.stdout")
	stderr=$(cat "$SCENARIO_TMP/run.stderr")
}

assert_eq ()
{
	# assert_eq EXPECTED ACTUAL [MESSAGE]
	if [ "$1" != "$2" ]; then
		fail "${3:-values differ}: expected [$1], got [$2]"
	fi
}

assert_status ()
{
	# assert_status EXPECTED (checks $status from the last 'run')
	if [ "$status" != "$1" ]; then
		fail "expected exit status $1, got $status; stdout=[$stdout] stderr=[$stderr]"
	fi
}

assert_contains ()
{
	# assert_contains HAYSTACK NEEDLE
	case "$1" in
		*"$2"*) ;;
		*) fail "expected output to contain [$2], got [$1]" ;;
	esac
}

assert_not_contains ()
{
	# assert_not_contains HAYSTACK NEEDLE
	case "$1" in
		*"$2"*) fail "expected output not to contain [$2], got [$1]" ;;
		*) ;;
	esac
}

# Prints the first 10 bytes of stdin as lowercase hex.
head_hex ()
{
	head -c 10 | od -An -tx1 | tr -d ' \n'
}

assert_blob_encrypted ()
{
	# assert_blob_encrypted OBJECT (e.g. HEAD:secret.txt or :secret.txt)
	local hex
	hex=$(git cat-file blob "$1" | head_hex)
	assert_eq "$GIT_CRYPT_HEADER_HEX" "$hex" "blob $1 is not git-crypt encrypted"
}

assert_blob_not_encrypted ()
{
	local hex
	hex=$(git cat-file blob "$1" | head_hex)
	if [ "$hex" = "$GIT_CRYPT_HEADER_HEX" ]; then
		fail "blob $1 is encrypted"
	fi
}

assert_file_encrypted ()
{
	# assert_file_encrypted PATH (a working tree file)
	local hex
	hex=$(head_hex < "$1")
	assert_eq "$GIT_CRYPT_HEADER_HEX" "$hex" "working tree file $1 is not encrypted"
}

assert_file_content ()
{
	# assert_file_content PATH EXPECTED
	[ -f "$1" ] || fail "$1 does not exist"
	assert_eq "$2" "$(cat "$1")" "content of $1"
}

# Creates a Git repository in DIR (default: repo) and cds into it.
new_repo ()
{
	local dir=${1:-repo}
	git -c init.defaultBranch=main init -q "$dir"
	cd "$dir"
}

# Creates a repository with git-crypt initialized and a committed
# .gitattributes that encrypts files named secret*.  Leaves the shell
# inside the repository.
new_encrypted_repo ()
{
	new_repo "$@"
	git-crypt init >/dev/null 2>&1
	echo 'secret* filter=git-crypt diff=git-crypt' > .gitattributes
	git add .gitattributes
	git commit -q -m 'Add .gitattributes'
}

require_gpg ()
{
	command -v gpg >/dev/null 2>&1 || skip "gpg not installed"
}

# gpg_gen_key NAME EMAIL: generates a passphrase-less key with an
# encryption subkey in $GNUPGHOME and prints its primary fingerprint.
gpg_gen_key ()
{
	require_gpg
	gpg --batch --quiet --passphrase '' --quick-gen-key "$1 <$2>" default default never 2>/dev/null
	gpg_fingerprint "$2"
}

# gpg_fingerprint USER_ID: prints the primary key fingerprint for USER_ID.
gpg_fingerprint ()
{
	gpg --batch --with-colons --fingerprint --list-keys "$1" | awk -F: '/^pub:/ { want = 1; next } want && /^fpr:/ { print $10; exit }'
}

# new_gnupghome: creates an additional, empty GNUPGHOME and prints its path.
# tests/run.sh stops its gpg-agent and removes it after the scenario.
new_gnupghome ()
{
	local home
	home=$(mktemp -d /tmp/gcg.XXXXXX)
	chmod 700 "$home"
	echo "$home" >> "$SCENARIO_TMP/extra-gnupghomes"
	echo "$home"
}

# gpg_add_subkey FPR ALGO USAGE: adds a subkey to the key FPR and prints the
# new subkey's fingerprint.
gpg_add_subkey ()
{
	gpg --batch --quiet --passphrase '' --quick-add-key "$1" "$2" "$3" never 2>/dev/null
	gpg --batch --with-colons --fingerprint --list-keys "$1" | awk -F: '/^fpr:/ { last = $10 } END { print last }'
}
