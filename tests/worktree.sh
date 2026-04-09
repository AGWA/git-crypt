#!/bin/sh

set -eu

if [ "$#" -ne 1 ]; then
	echo "Usage: $0 GIT_CRYPT_BINARY" >&2
	exit 2
fi

GIT_CRYPT=$1

case "$GIT_CRYPT" in
/*) ;;
*)
	GIT_CRYPT=$(cd "$(dirname "$GIT_CRYPT")" && pwd)/$(basename "$GIT_CRYPT")
	;;
esac

if [ ! -x "$GIT_CRYPT" ]; then
	echo "git-crypt binary is not executable: $GIT_CRYPT" >&2
	exit 1
fi

TMP_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/git-crypt-worktree-test.XXXXXX")
trap 'rm -rf "$TMP_ROOT"' EXIT INT TERM HUP

fail () {
	echo "not ok - $1" >&2
	exit 1
}

assert_file_equals () {
	path=$1
	expected=$2

	if [ ! -f "$path" ]; then
		fail "missing file: $path"
	fi

	actual=$(cat "$path")
	if [ "$actual" != "$expected" ]; then
		fail "unexpected contents in $path"
	fi
}

assert_clean_status () {
	repo=$1
	status=$(git -C "$repo" status --short)
	if [ -n "$status" ]; then
		echo "$status" >&2
		fail "working tree is not clean: $repo"
	fi
}

init_repo () {
	repo=$1

	mkdir -p "$repo"
	git -C "$repo" init >/dev/null
	git -C "$repo" config user.name "Codex Test"
	git -C "$repo" config user.email "codex@example.com"
	git -C "$repo" config core.fsmonitor false

	(
		cd "$repo"
		key_path=$(cd .. && pwd)/$(basename "$repo").key
		"$GIT_CRYPT" init >/dev/null
		printf 'secret.txt filter=git-crypt diff=git-crypt\n' > .gitattributes
		printf 'topsecret\n' > secret.txt
		git add .gitattributes secret.txt
		git commit -m "init secret" >/dev/null
		"$GIT_CRYPT" export-key "$key_path" >/dev/null
	)
}

test_non_worktree_unlock () {
	repo=$TMP_ROOT/non-worktree

	init_repo "$repo"

	(
		cd "$repo"
		"$GIT_CRYPT" lock --force >/dev/null
		"$GIT_CRYPT" unlock "$repo.key" >/dev/null
	)

	assert_file_equals "$repo/secret.txt" "topsecret"
	assert_clean_status "$repo"
}

test_worktree_checkout_uses_common_state () {
	repo=$TMP_ROOT/worktree-main
	wt=$TMP_ROOT/worktree-checkout

	init_repo "$repo"
	git -C "$repo" worktree add "$wt" >/dev/null
	git -C "$wt" config core.fsmonitor false

	assert_file_equals "$wt/secret.txt" "topsecret"
	assert_clean_status "$wt"
}

test_worktree_unlock_uses_common_state () {
	repo=$TMP_ROOT/worktree-unlock-main
	wt=$TMP_ROOT/worktree-unlock

	mkdir -p "$repo"
	git -C "$repo" init >/dev/null
	git -C "$repo" config user.name "Codex Test"
	git -C "$repo" config user.email "codex@example.com"
	git -C "$repo" config core.fsmonitor false

	(
		cd "$repo"
		printf 'base\n' > README
		git add README
		git commit -m "base" >/dev/null

		key_path=$(cd .. && pwd)/$(basename "$repo").key
		"$GIT_CRYPT" init >/dev/null
		printf 'secret.txt filter=git-crypt diff=git-crypt\n' > .gitattributes
		printf 'topsecret\n' > secret.txt
		git add .gitattributes secret.txt
		git commit -m "add secret" >/dev/null
		encrypted_rev=$(git rev-parse HEAD)
		printf '%s\n' "$encrypted_rev" > ../$(basename "$repo").rev
		"$GIT_CRYPT" export-key "$key_path" >/dev/null
		"$GIT_CRYPT" lock --force >/dev/null
	)

	git -C "$repo" worktree add --detach "$wt" HEAD~1 >/dev/null
	git -C "$wt" config core.fsmonitor false
	encrypted_rev=$(cat "$TMP_ROOT/$(basename "$repo").rev")

	(
		cd "$wt"
		"$GIT_CRYPT" unlock "$repo.key" >/dev/null
		git checkout "$encrypted_rev" >/dev/null
	)

	common_dir=$(git -C "$wt" rev-parse --git-common-dir)
	git_dir=$(git -C "$wt" rev-parse --git-dir)

	if [ ! -f "$common_dir/git-crypt/keys/default" ]; then
		fail "missing common-dir key state"
	fi

	if [ -e "$git_dir/git-crypt/keys/default" ]; then
		fail "unexpected worktree-local key state"
	fi

	assert_file_equals "$wt/secret.txt" "topsecret"
	assert_clean_status "$wt"
}

test_non_worktree_unlock
test_worktree_checkout_uses_common_state
test_worktree_unlock_uses_common_state

echo "ok - worktree regression tests passed"
