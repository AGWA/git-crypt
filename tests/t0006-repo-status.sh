# git-crypt status -r: report whether each key is locked or unlocked.

# A repository with the default key and a "prod" key, each encrypting one
# committed file, and both keys exported to $SCENARIO_TMP.
new_two_key_repo ()
{
	new_repo
	git-crypt init >/dev/null 2>&1
	git-crypt init -k prod >/dev/null 2>&1
	printf 'secret* filter=git-crypt diff=git-crypt\nprod* filter=git-crypt-prod diff=git-crypt-prod\n' > .gitattributes
	echo 'default secret' > secret.txt
	echo 'prod secret' > prod.txt
	git add .gitattributes secret.txt prod.txt
	git commit -q -m 'Add files'
	git-crypt export-key "$SCENARIO_TMP/default.key"
	git-crypt export-key -k prod "$SCENARIO_TMP/prod.key"
}

scenario_unlocked_repo ()
{
	new_encrypted_repo
	run git-crypt status -r
	assert_status 0
	assert_eq "default: unlocked" "$stdout"
}

scenario_locked_repo ()
{
	new_encrypted_repo
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
	git-crypt lock
	run git-crypt status -r
	assert_status 1
	assert_eq "default: locked" "$stdout"
}

scenario_fresh_clone_of_symmetric_key_repo_is_locked ()
{
	new_encrypted_repo origin
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
	cd "$SCENARIO_TMP"
	git clone -q origin clone
	cd clone
	run git-crypt status -r
	assert_status 1
	assert_eq "default: locked" "$stdout"
}

scenario_mixed_multi_key_repo ()
{
	new_two_key_repo
	run git-crypt status -r
	assert_status 0
	assert_eq "$(printf 'default: unlocked\nprod: unlocked')" "$stdout"

	git-crypt lock -k prod
	run git-crypt status -r
	assert_status 1
	assert_eq "$(printf 'default: unlocked\nprod: locked')" "$stdout"

	git-crypt lock
	run git-crypt status -r
	assert_status 1
	assert_eq "$(printf 'default: locked\nprod: locked')" "$stdout"
}

scenario_key_with_filter_configured_but_no_key_is_locked ()
{
	new_encrypted_repo
	rm .git/git-crypt/keys/default
	run git-crypt status -r
	assert_status 1
	assert_eq "default: locked" "$stdout"
}

scenario_same_answer_from_linked_worktree_and_subdirectory ()
{
	new_two_key_repo
	git-crypt lock -k prod
	mkdir sub
	git worktree add -q "$SCENARIO_TMP/wt" -b other
	local expected
	expected=$(printf 'default: unlocked\nprod: locked')

	(cd sub && run git-crypt status -r && assert_status 1 && assert_eq "$expected" "$stdout")
	(cd "$SCENARIO_TMP/wt" && run git-crypt status -r && assert_status 1 && assert_eq "$expected" "$stdout")
}

scenario_repo_without_git_crypt ()
{
	new_repo
	echo hi > file.txt
	git add file.txt
	git commit -q -m 'Add file'
	run git-crypt status -r
	assert_status 0
	assert_eq "No git-crypt keys found." "$stdout"
}

scenario_quiet_suppresses_stdout_but_keeps_exit_status ()
{
	new_encrypted_repo
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
	run git-crypt status -r -q
	assert_status 0
	assert_eq "" "$stdout"

	git-crypt lock
	run git-crypt status -r --quiet
	assert_status 1
	assert_eq "" "$stdout"
}

scenario_quiet_plain_status ()
{
	new_repo
	git-crypt init >/dev/null 2>&1
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'plaintext before .gitattributes'
	echo 'secret* filter=git-crypt diff=git-crypt' > .gitattributes
	git add .gitattributes
	git commit -q -m 'Add .gitattributes'

	run git-crypt status -q
	assert_status 1
	assert_eq "" "$stdout"

	run git-crypt status -q -f
	assert_status 0
	assert_eq "" "$stdout"
	assert_blob_encrypted :secret.txt
}

scenario_r_with_other_options_is_a_usage_error ()
{
	new_encrypted_repo
	for args in "-r -e" "-r -u" "-r -f" "-r secret.txt"; do
		run git-crypt status $args
		assert_status 2
	done
}

scenario_never_prints_key_material ()
{
	new_encrypted_repo
	run git-crypt status -r
	local key_hex
	key_hex=$(od -An -tx1 < .git/git-crypt/keys/default | tr -d ' \n')
	assert_not_contains "$(printf '%s%s' "$stdout" "$stderr" | od -An -tx1 | tr -d ' \n')" "${key_hex:40:32}"
}
