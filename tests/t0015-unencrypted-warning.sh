# The "file not encrypted" warning printed by smudge.

WARNING="git-crypt: warning: file not encrypted (run 'git-crypt status' for details)"

scenario_warning_is_one_line ()
{
	new_encrypted_repo
	printf 'plain text\n' > "$SCENARIO_TMP/plain"
	run sh -c "git-crypt smudge < '$SCENARIO_TMP/plain' > '$SCENARIO_TMP/out'"
	assert_status 0
	assert_eq "$WARNING" "$stderr" "stderr"
	cmp -s "$SCENARIO_TMP/plain" "$SCENARIO_TMP/out" || fail "output changed"
}

scenario_warning_can_be_disabled ()
{
	new_encrypted_repo
	git config git-crypt.warnUnencrypted false
	printf 'plain text\n' > "$SCENARIO_TMP/plain"
	run sh -c "git-crypt smudge < '$SCENARIO_TMP/plain' > '$SCENARIO_TMP/out'"
	assert_status 0
	assert_eq "" "$stderr" "stderr"
	cmp -s "$SCENARIO_TMP/plain" "$SCENARIO_TMP/out" || fail "output changed"

	git config git-crypt.warnUnencrypted true
	run sh -c "git-crypt smudge < '$SCENARIO_TMP/plain'"
	assert_eq "$WARNING" "$stderr" "stderr with warnUnencrypted=true"
}

scenario_invalid_setting_keeps_warning ()
{
	new_encrypted_repo
	git config git-crypt.warnUnencrypted bogus
	run sh -c "printf 'plain\n' | git-crypt smudge"
	assert_status 0
	assert_eq "plain" "$stdout"
	assert_contains "$stderr" "file not encrypted"
}

scenario_checkout_of_plaintext_blob_is_quiet_when_disabled ()
{
	new_repo
	git-crypt init >/dev/null 2>&1
	echo 'v1' > secret.txt
	git add secret.txt
	git commit -q -m 'plaintext'
	echo 'secret* filter=git-crypt diff=git-crypt' > .gitattributes
	git add .gitattributes
	git commit -q -m 'Add .gitattributes'
	git config git-crypt.warnUnencrypted false

	rm secret.txt
	run git checkout -- secret.txt
	assert_status 0
	assert_eq "" "$stderr" "stderr of checkout"
	assert_file_content secret.txt 'v1'
}
