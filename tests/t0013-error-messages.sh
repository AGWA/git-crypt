# Common key/unlock failures say what failed, where, and what to do next.

new_repo_with_secret ()
{
	new_encrypted_repo "$@"
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
}

scenario_init_refuses_in_clone_of_gpg_repo ()
{
	local fpr
	fpr=$(gpg_gen_key 'Alice' alice@example.com)
	new_repo_with_secret origin
	git-crypt add-gpg-user --trusted "$fpr" >/dev/null 2>&1
	cd "$SCENARIO_TMP"
	git clone -q origin clone
	cd clone

	run git-crypt init
	assert_status 1
	assert_contains "$stderr" "already"
	assert_contains "$stderr" "git-crypt unlock"
	[ ! -e .git/git-crypt/keys/default ] || fail "init generated a key anyway"
}

scenario_init_refuses_in_clone_of_symmetric_key_repo ()
{
	new_repo_with_secret origin
	cd "$SCENARIO_TMP"
	git clone -q origin clone
	cd clone

	run git-crypt init
	assert_status 1
	assert_contains "$stderr" "git-crypt unlock"
	[ ! -e .git/git-crypt/keys/default ] || fail "init generated a key anyway"
}

scenario_init_force_reinitializes_deliberately ()
{
	new_repo_with_secret origin
	cd "$SCENARIO_TMP"
	git clone -q origin clone
	cd clone
	run git-crypt init --force
	assert_status 0
	[ -f .git/git-crypt/keys/default ] || fail "no key generated"
}

scenario_init_of_other_key_is_allowed ()
{
	new_repo_with_secret origin
	cd "$SCENARIO_TMP"
	git clone -q origin clone
	cd clone
	run git-crypt init -k prod
	assert_status 0
}

scenario_failed_unlock_suggests_lock_force ()
{
	new_repo_with_secret
	git-crypt lock
	git-crypt keygen "$SCENARIO_TMP/wrong-key" 2>/dev/null

	run git-crypt unlock "$SCENARIO_TMP/wrong-key"
	assert_status 1
	assert_contains "$stderr" "git-crypt lock -a --force"

	# Following the advice returns to a clean locked state.
	git-crypt lock -a --force
	run git-crypt status -r
	assert_eq "default: locked" "$stdout"
	assert_file_encrypted secret.txt
}

scenario_malformed_collaborator_file_names_file_and_key ()
{
	local fpr
	fpr=$(gpg_gen_key 'Alice' alice@example.com)
	new_repo_with_secret origin
	mkdir -p .git-crypt/keys/prod/0
	printf '* !filter !diff\n*.gpg binary\n' > .git-crypt/.gitattributes
	head -c 200 /dev/urandom | gpg --batch --trust-model always -r "$fpr" -e > ".git-crypt/keys/prod/0/$fpr.gpg"
	git add .git-crypt
	git commit -q -m 'Add a corrupt collaborator file'
	cd "$SCENARIO_TMP"
	git clone -q origin clone
	cd clone

	run git-crypt unlock
	assert_status 1
	assert_contains "$stderr" ".git-crypt/keys/prod/0/$fpr.gpg"
	assert_contains "$stderr" "'prod'"
}

scenario_malformed_internal_key_names_file_and_key ()
{
	new_encrypted_repo
	printf 'garbage' > .git/git-crypt/keys/default
	run git-crypt export-key "$SCENARIO_TMP/out"
	assert_status 1
	assert_contains "$stderr" ".git/git-crypt/keys/default"
	assert_contains "$stderr" "'default'"
}

scenario_missing_internal_key_names_the_key ()
{
	new_repo
	run git-crypt export-key -k prod "$SCENARIO_TMP/out"
	assert_status 1
	assert_contains "$stderr" "'prod'"
}

scenario_unlock_with_directory ()
{
	new_repo_with_secret
	git-crypt lock
	mkdir "$SCENARIO_TMP/somedir"
	run git-crypt unlock "$SCENARIO_TMP/somedir"
	assert_status 1
	assert_contains "$stderr" "is a directory"
}

scenario_unlock_with_missing_file ()
{
	new_repo_with_secret
	git-crypt lock
	run git-crypt unlock "$SCENARIO_TMP/nonexistent"
	assert_status 1
	assert_contains "$stderr" "$SCENARIO_TMP/nonexistent"
	assert_contains "$stderr" "No such file"
}

scenario_unlock_with_encrypted_file ()
{
	new_repo_with_secret
	git-crypt lock
	run git-crypt unlock secret.txt
	assert_status 1
	assert_contains "$stderr" "encrypted file, not a key file"
}

scenario_unlock_with_non_key_file ()
{
	new_repo_with_secret
	git-crypt lock
	echo 'not a key' > "$SCENARIO_TMP/notakey"
	run git-crypt unlock "$SCENARIO_TMP/notakey"
	assert_status 1
	assert_contains "$stderr" "not a git-crypt key file"
	assert_not_contains "$stderr" "migrate-key"
}

scenario_unlock_with_legacy_sized_file_suggests_migrate_key ()
{
	new_repo_with_secret
	git-crypt lock
	head -c 96 /dev/urandom > "$SCENARIO_TMP/legacy"
	run git-crypt unlock "$SCENARIO_TMP/legacy"
	assert_status 1
	assert_contains "$stderr" "migrate-key"
}

scenario_add_gpg_user_with_unusable_key_gives_hint ()
{
	require_gpg
	gpg --batch --quiet --passphrase '' --quick-gen-key 'Signer <signer@example.com>' ed25519 sign never 2>/dev/null
	local fpr
	fpr=$(gpg_fingerprint signer@example.com)
	new_repo_with_secret
	run git-crypt add-gpg-user --trusted "$fpr"
	assert_status 1
	assert_contains "$stderr" "expired"
	assert_contains "$stderr" "encryption subkey"
}

scenario_errors_never_print_key_material ()
{
	new_encrypted_repo
	local key_hex
	key_hex=$(od -An -tx1 < .git/git-crypt/keys/default | tr -d ' \n')
	head -c 40 .git/git-crypt/keys/default > "$SCENARIO_TMP/truncated"
	run git-crypt unlock "$SCENARIO_TMP/truncated"
	assert_status 1
	assert_not_contains "$(printf '%s%s' "$stdout" "$stderr" | od -An -tx1 | tr -d ' \n')" "${key_hex:60:32}"
}

scenario_unlock_from_stdin_with_encrypted_file ()
{
	new_repo_with_secret
	git cat-file blob HEAD:secret.txt > "$SCENARIO_TMP/blob"
	git-crypt lock
	run sh -c "git-crypt unlock - < '$SCENARIO_TMP/blob'"
	assert_status 1
	assert_contains "$stderr" "- is a git-crypt encrypted file, not a key file"
}
