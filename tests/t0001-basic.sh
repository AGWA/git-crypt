# Basic encrypt / lock / unlock workflows with a symmetric key.

scenario_committed_file_is_encrypted ()
{
	new_encrypted_repo
	echo 'top secret' > secret.txt
	echo 'public' > public.txt
	git add secret.txt public.txt
	git commit -q -m 'Add files'

	assert_blob_encrypted HEAD:secret.txt
	assert_blob_not_encrypted HEAD:public.txt
	assert_file_content secret.txt 'top secret'
}

scenario_lock_then_unlock_with_exported_key ()
{
	new_encrypted_repo
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
	git-crypt export-key "$SCENARIO_TMP/key"

	git-crypt lock
	assert_file_encrypted secret.txt

	git-crypt unlock "$SCENARIO_TMP/key"
	assert_file_content secret.txt 'top secret'
	assert_eq "" "$(git status --porcelain)" "working tree clean after unlock"
}

scenario_status_reports_encrypted_and_unencrypted_files ()
{
	new_encrypted_repo
	echo 'top secret' > secret.txt
	echo 'public' > public.txt
	git add secret.txt public.txt
	git commit -q -m 'Add files'

	run git-crypt status
	assert_status 0
	assert_contains "$stdout" "    encrypted: secret.txt"
	assert_contains "$stdout" "not encrypted: public.txt"
	assert_contains "$stdout" "not encrypted: .gitattributes"

	run git-crypt status -e
	assert_eq "    encrypted: secret.txt" "$stdout" "status -e output"

	run git-crypt status -u
	assert_not_contains "$stdout" "secret.txt"
}

scenario_status_flags_plaintext_committed_before_gitattributes ()
{
	new_repo
	git-crypt init >/dev/null 2>&1
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret before .gitattributes'
	echo 'secret* filter=git-crypt diff=git-crypt' > .gitattributes
	git add .gitattributes
	git commit -q -m 'Add .gitattributes'

	run git-crypt status
	assert_status 1
	assert_contains "$stdout" "secret.txt *** WARNING: staged/committed version is NOT ENCRYPTED! ***"

	run git-crypt status -f
	assert_status 0
	assert_contains "$stdout" "secret.txt: staged encrypted version"
	assert_blob_encrypted :secret.txt
}

scenario_unlock_reads_key_from_stdin ()
{
	new_encrypted_repo
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
	git-crypt export-key - > "$SCENARIO_TMP/key"
	git-crypt lock

	git-crypt unlock - < "$SCENARIO_TMP/key"
	assert_file_content secret.txt 'top secret'
}

scenario_legacy_init_with_key_file_unlocks ()
{
	new_encrypted_repo
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
	git-crypt export-key "$SCENARIO_TMP/key"
	git-crypt lock

	git-crypt init "$SCENARIO_TMP/key" 2>/dev/null
	assert_file_content secret.txt 'top secret'
}
