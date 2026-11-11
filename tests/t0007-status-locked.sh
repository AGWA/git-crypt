# status and status -f must not try to "fix" files whose key is locked.
# (The unlocked status -f path is covered in t0001-basic.sh.)

# A locked repository in which secret-b was staged in plaintext (possible
# because a locked repository has no git-crypt filter configured).
new_locked_repo_with_plaintext_staged ()
{
	new_encrypted_repo "$@"
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
	git-crypt export-key "$SCENARIO_TMP/key"
	git-crypt lock
	echo 'new secret' > secret-b
	git add secret-b
	assert_blob_not_encrypted :secret-b
}

scenario_fix_refuses_when_locked_and_leaves_index_unchanged ()
{
	new_locked_repo_with_plaintext_staged
	local index_before
	index_before=$(git ls-files -s)

	run git-crypt status -f
	assert_status 1
	assert_contains "$stderr" "secret-b"
	assert_contains "$stderr" "locked"
	assert_contains "$stderr" "git-crypt unlock"
	assert_not_contains "$stderr" "still unencrypted even after staging"
	assert_eq "$index_before" "$(git ls-files -s)" "index changed"
}

scenario_plain_status_hints_to_unlock_when_locked ()
{
	new_locked_repo_with_plaintext_staged
	run git-crypt status
	assert_status 1
	assert_contains "$stdout" "secret-b *** WARNING: staged/committed version is NOT ENCRYPTED! ***"
	assert_contains "$stdout" "git-crypt unlock"
}

scenario_plain_status_does_not_hint_to_unlock_when_unlocked ()
{
	new_repo
	git-crypt init >/dev/null 2>&1
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'plaintext before .gitattributes'
	echo 'secret* filter=git-crypt diff=git-crypt' > .gitattributes
	git add .gitattributes
	git commit -q -m 'Add .gitattributes'

	run git-crypt status
	assert_status 1
	assert_contains "$stdout" "with the '-f' option"
	assert_not_contains "$stdout" "git-crypt unlock"
}

scenario_fix_refuses_from_linked_worktree ()
{
	new_encrypted_repo
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
	git worktree add -q "$SCENARIO_TMP/wt" -b other
	git-crypt lock
	cd "$SCENARIO_TMP/wt"
	echo 'new secret' > secret-b
	git add secret-b

	run git-crypt status -f
	assert_status 1
	assert_contains "$stderr" "git-crypt unlock"
	assert_blob_not_encrypted :secret-b
}

scenario_fix_names_the_locked_key ()
{
	new_repo
	git-crypt init -k prod >/dev/null 2>&1
	echo 'prod* filter=git-crypt-prod diff=git-crypt-prod' > .gitattributes
	git add .gitattributes
	git commit -q -m 'Add .gitattributes'
	git-crypt lock -k prod
	echo 'prod secret' > prod.txt
	git add prod.txt

	run git-crypt status -f
	assert_status 1
	assert_contains "$stderr" "'prod'"
	assert_contains "$stderr" "git-crypt unlock"

	run git-crypt status
	assert_contains "$stdout" "'prod'"
}

scenario_recovery_path_from_the_hint_works ()
{
	new_locked_repo_with_plaintext_staged
	git stash -q
	git-crypt unlock "$SCENARIO_TMP/key"
	git stash pop -q 2>/dev/null
	run git-crypt status -f
	assert_status 0
	assert_contains "$stdout" "secret-b: staged encrypted version"
	assert_blob_encrypted :secret-b
}
