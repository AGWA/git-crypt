# Key names: "-k default" must behave exactly like omitting -k.

commit_secret ()
{
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
}

scenario_init_k_default_initializes_the_default_key ()
{
	new_repo
	git-crypt init -k default
	git config --get filter.git-crypt.smudge >/dev/null || fail "default filter not configured"
	[ -z "$(git config --get-regexp '^filter\.git-crypt-default\.' || true)" ] || fail "configured a filter named git-crypt-default"
	[ -f .git/git-crypt/keys/default ] || fail "default key not created"
}

scenario_export_key_k_default_matches_export_key ()
{
	new_encrypted_repo
	git-crypt export-key "$SCENARIO_TMP/plain-key"
	git-crypt export-key -k default "$SCENARIO_TMP/default-key"
	cmp -s "$SCENARIO_TMP/plain-key" "$SCENARIO_TMP/default-key" || fail "export-key -k default differs"
}

scenario_lock_k_default_deconfigures_the_default_filter ()
{
	new_encrypted_repo
	commit_secret
	git-crypt lock -k default
	run git config --get-regexp '^(filter|diff)\.git-crypt'
	assert_eq "" "$stdout" "git-crypt config left after lock -k default"
	assert_file_encrypted secret.txt
}

scenario_add_gpg_user_k_default_then_clone_and_unlock ()
{
	local fpr
	fpr=$(gpg_gen_key 'Alice' alice@example.com)

	new_encrypted_repo origin
	commit_secret
	git-crypt add-gpg-user -k default --trusted "$fpr"
	[ -f ".git-crypt/keys/default/0/$fpr.gpg" ] || fail "collaborator file not written to default/"

	cd "$SCENARIO_TMP"
	git clone -q origin clone
	cd clone
	git-crypt unlock
	assert_file_content secret.txt 'top secret'
}

# Before the fix, "add-gpg-user -k default" stored the default key with a
# key name field of "default".  Such collaborator files must still unlock.
scenario_unlock_accepts_collaborator_file_written_by_old_k_default ()
{
	local fpr
	fpr=$(gpg_gen_key 'Alice' alice@example.com)

	new_encrypted_repo origin
	commit_secret
	git-crypt export-key "$SCENARIO_TMP/key"
	{
		head -c 16 "$SCENARIO_TMP/key"                  # magic + format version
		printf '\000\000\000\001\000\000\000\007default'  # key name field = "default"
		tail -c +17 "$SCENARIO_TMP/key"                 # end of header + entries
	} > "$SCENARIO_TMP/old-style-key"
	mkdir -p .git-crypt/keys/default/0
	printf '* !filter !diff\n*.gpg binary\n' > .git-crypt/.gitattributes
	gpg --batch --trust-model always -o ".git-crypt/keys/default/0/$fpr.gpg" -r "$fpr" -e "$SCENARIO_TMP/old-style-key"
	git add .git-crypt
	git commit -q -m 'Add collaborator the old way'

	cd "$SCENARIO_TMP"
	git clone -q origin clone
	cd clone
	git-crypt unlock
	assert_file_content secret.txt 'top secret'
	[ -f .git/git-crypt/keys/default ] || fail "unlocked as a key other than default"
}

scenario_named_keys_are_unchanged ()
{
	new_repo
	git-crypt init -k prod
	git config --get filter.git-crypt-prod.smudge >/dev/null || fail "git-crypt-prod filter not configured"
	[ -f .git/git-crypt/keys/prod ] || fail "prod key not created"
	git-crypt lock -k prod
	run git config --get-regexp '^filter\.git-crypt-prod'
	assert_eq "" "$stdout" "prod filter left after lock -k prod"
}
