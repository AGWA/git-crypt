# lock and unlock act on every linked worktree.

scenario_lock_and_unlock_affect_linked_worktrees ()
{
	new_encrypted_repo
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
	git-crypt export-key "$SCENARIO_TMP/key"
	git worktree add -q "$SCENARIO_TMP/wt" -b other
	assert_file_content "$SCENARIO_TMP/wt/secret.txt" 'top secret'

	git-crypt lock
	assert_file_encrypted secret.txt
	assert_file_encrypted "$SCENARIO_TMP/wt/secret.txt"

	# unlock from inside the linked worktree
	cd "$SCENARIO_TMP/wt"
	git-crypt unlock "$SCENARIO_TMP/key"
	assert_file_content secret.txt 'top secret'
	assert_file_content "$SCENARIO_TMP/repo/secret.txt" 'top secret'
}
