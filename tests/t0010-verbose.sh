# unlock -v / lock -v list the files they decrypted / re-encrypted.

new_locked_two_key_repo ()
{
	new_repo
	git-crypt init >/dev/null 2>&1
	git-crypt init -k prod >/dev/null 2>&1
	printf 'secret* filter=git-crypt diff=git-crypt\nprod* filter=git-crypt-prod diff=git-crypt-prod\n' > .gitattributes
	mkdir dir
	echo 'default secret' > secret.txt
	echo 'another secret' > dir/secret2
	echo 'prod secret' > prod.txt
	echo 'public' > public.txt
	git add .
	git commit -q -m 'Add files'
	git-crypt export-key "$SCENARIO_TMP/default.key"
	git-crypt export-key -k prod "$SCENARIO_TMP/prod.key"
	git-crypt lock -a
}

scenario_unlock_v_lists_decrypted_files_per_key ()
{
	new_locked_two_key_repo
	run git-crypt unlock -v "$SCENARIO_TMP/default.key" "$SCENARIO_TMP/prod.key"
	assert_status 0
	assert_eq "$(printf "Decrypted with key 'default':\n    dir/secret2\n    secret.txt\nDecrypted with key 'prod':\n    prod.txt")" "$stdout"
	assert_file_content prod.txt 'prod secret'
}

scenario_unlock_without_v_prints_nothing ()
{
	new_locked_two_key_repo
	run git-crypt unlock "$SCENARIO_TMP/default.key"
	assert_status 0
	assert_eq "" "$stdout"
}

scenario_lock_v_lists_reencrypted_files ()
{
	new_locked_two_key_repo
	git-crypt unlock "$SCENARIO_TMP/default.key" "$SCENARIO_TMP/prod.key"
	run git-crypt lock -v -k prod
	assert_status 0
	assert_eq "$(printf "Encrypted with key 'prod':\n    prod.txt")" "$stdout"

	run git-crypt lock --verbose
	assert_eq "$(printf "Encrypted with key 'default':\n    dir/secret2\n    secret.txt")" "$stdout"
}

scenario_key_matching_no_files ()
{
	new_encrypted_repo
	git-crypt export-key "$SCENARIO_TMP/key"
	git-crypt lock
	run git-crypt unlock -v "$SCENARIO_TMP/key"
	assert_status 0
	assert_eq "$(printf "Decrypted with key 'default':\n    (no files)")" "$stdout"
}

scenario_unlock_v_lists_files_per_worktree ()
{
	new_locked_two_key_repo
	git worktree add -q "$SCENARIO_TMP/wt" -b other
	local main_wt linked_wt
	main_wt=$(git rev-parse --show-toplevel)
	linked_wt=$(cd "$SCENARIO_TMP/wt" && git rev-parse --show-toplevel)

	run git-crypt unlock -v "$SCENARIO_TMP/prod.key"
	assert_status 0
	assert_eq "$(printf "Decrypted with key 'prod' in %s:\n    prod.txt\nDecrypted with key 'prod' in %s:\n    prod.txt" "$main_wt" "$linked_wt")" "$stdout"
}

scenario_gpg_unlock_v ()
{
	local fpr
	fpr=$(gpg_gen_key 'Alice' alice@example.com)
	new_encrypted_repo origin
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
	git-crypt add-gpg-user --trusted "$fpr" >/dev/null 2>&1
	cd "$SCENARIO_TMP"
	git clone -q origin clone
	cd clone
	run git-crypt unlock -v
	assert_status 0
	assert_eq "$(printf "Decrypted with key 'default':\n    secret.txt")" "$stdout"
}
