# git-crypt lock --if-unlocked: idempotent locking.

new_repo_with_secret ()
{
	new_encrypted_repo "$@"
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
}

scenario_already_locked_exits_zero_silently ()
{
	new_repo_with_secret
	git-crypt lock
	run git-crypt lock --if-unlocked
	assert_status 0
	assert_eq "" "$stdout$stderr" "output"
}

scenario_already_locked_with_dirty_tree_exits_zero ()
{
	new_repo_with_secret
	git-crypt lock
	echo 'unrelated change' >> .gitattributes
	run git-crypt lock --if-unlocked
	assert_status 0
	assert_contains "$(cat .gitattributes)" 'unrelated change'
}

scenario_already_locked_from_linked_worktree ()
{
	new_repo_with_secret
	git worktree add -q "$SCENARIO_TMP/wt" -b other
	git-crypt lock
	cd "$SCENARIO_TMP/wt"
	run git-crypt lock --if-unlocked
	assert_status 0
}

scenario_unlocked_repo_is_locked_like_plain_lock ()
{
	new_repo_with_secret
	run git-crypt lock --if-unlocked
	assert_status 0
	assert_file_encrypted secret.txt
	[ ! -e .git/git-crypt/keys/default ] || fail "key not removed"
	run git config --get-regexp '^(filter|diff)\.git-crypt'
	assert_eq "" "$stdout" "filters left configured"
}

scenario_unlocked_dirty_repo_still_refuses ()
{
	new_repo_with_secret
	echo 'uncommitted' >> secret.txt
	run git-crypt lock --if-unlocked
	assert_status 1
	assert_contains "$stderr" "Working directory not clean"
}

scenario_plain_lock_on_locked_repo_still_fails ()
{
	new_repo_with_secret
	git-crypt lock
	run git-crypt lock
	assert_status 1
	assert_contains "$stderr" "already locked"
}

scenario_named_key ()
{
	new_repo
	git-crypt init >/dev/null 2>&1
	git-crypt init -k prod >/dev/null 2>&1
	printf 'secret* filter=git-crypt diff=git-crypt\nprod* filter=git-crypt-prod diff=git-crypt-prod\n' > .gitattributes
	echo 'default secret' > secret.txt
	echo 'prod secret' > prod.txt
	git add .
	git commit -q -m 'Add files'

	git-crypt lock -k prod --if-unlocked
	assert_file_encrypted prod.txt
	run git-crypt lock -k prod --if-unlocked
	assert_status 0
	assert_file_content secret.txt 'default secret'
}

scenario_all_keys ()
{
	new_repo
	git-crypt init >/dev/null 2>&1
	git-crypt init -k prod >/dev/null 2>&1
	printf 'secret* filter=git-crypt diff=git-crypt\nprod* filter=git-crypt-prod diff=git-crypt-prod\n' > .gitattributes
	echo 'default secret' > secret.txt
	echo 'prod secret' > prod.txt
	git add .
	git commit -q -m 'Add files'

	git-crypt lock -k prod
	# One key still unlocked: -a locks it.
	run git-crypt lock -a --if-unlocked
	assert_status 0
	assert_file_encrypted secret.txt
	# Now every key is locked.
	echo 'dirty' >> .gitattributes
	run git-crypt lock -a --if-unlocked
	assert_status 0
}

scenario_all_keys_on_never_unlocked_clone ()
{
	new_repo_with_secret origin
	cd "$SCENARIO_TMP"
	git clone -q origin clone
	cd clone
	run git-crypt lock -a --if-unlocked
	assert_status 0
}
