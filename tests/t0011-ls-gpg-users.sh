# git-crypt ls-gpg-users: list the GPG collaborators of each key.

# Sets up a repository with Alice and Bob on the default key and Alice on
# the "prod" key.  Sets $alice and $bob to their fingerprints.
new_repo_with_collaborators ()
{
	alice=$(gpg_gen_key 'Alice' alice@example.com)
	bob=$(gpg_gen_key 'Bob' bob@example.com)
	new_encrypted_repo
	git-crypt init -k prod >/dev/null 2>&1
	git-crypt add-gpg-user --trusted "$alice" "$bob" >/dev/null 2>&1
	git-crypt add-gpg-user -k prod --trusted "$alice" >/dev/null 2>&1
}

# expected_default_lines: Alice and Bob's lines, sorted by fingerprint
expected_default_lines ()
{
	printf '    %s Alice <alice@example.com>\n    %s Bob <bob@example.com>\n' "$alice" "$bob" | sort
}

scenario_lists_collaborators_grouped_by_key_and_version ()
{
	new_repo_with_collaborators
	run git-crypt ls-gpg-users
	assert_status 0
	assert_eq "$(printf "Key 'default', version 0:\n%s\nKey 'prod', version 0:\n    %s Alice <alice@example.com>" "$(expected_default_lines)" "$alice")" "$stdout"
}

scenario_k_restricts_to_one_key ()
{
	new_repo_with_collaborators
	run git-crypt ls-gpg-users -k prod
	assert_status 0
	assert_eq "$(printf "Key 'prod', version 0:\n    %s Alice <alice@example.com>" "$alice")" "$stdout"

	run git-crypt ls-gpg-users -k default
	assert_eq "$(printf "Key 'default', version 0:\n%s" "$(expected_default_lines)")" "$stdout"
}

scenario_works_when_locked_and_from_linked_worktree ()
{
	new_repo_with_collaborators
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
	git-crypt lock -a
	git worktree add -q "$SCENARIO_TMP/wt" -b other

	run git-crypt ls-gpg-users
	assert_status 0
	assert_contains "$stdout" "$bob Bob <bob@example.com>"

	cd "$SCENARIO_TMP/wt"
	run git-crypt ls-gpg-users
	assert_status 0
	assert_contains "$stdout" "$bob Bob <bob@example.com>"
}

scenario_unknown_fingerprint_is_displayed_without_error ()
{
	require_gpg
	gpg --batch --list-keys >/dev/null 2>&1 # create the keyring now, so gpg's first-use notices don't show up below
	new_encrypted_repo
	mkdir -p .git-crypt/keys/default/0
	: > .git-crypt/keys/default/0/0123456789ABCDEF0123456789ABCDEF01234567.gpg
	run git-crypt ls-gpg-users
	assert_status 0
	assert_eq "$(printf "Key 'default', version 0:\n    0123456789ABCDEF0123456789ABCDEF01234567 (not in your GPG keyring)")" "$stdout"
	assert_eq "" "$stderr" "stderr"
}

scenario_no_collaborators ()
{
	new_encrypted_repo
	run git-crypt ls-gpg-users
	assert_status 0
	assert_eq "This repository has no GPG collaborators." "$stdout"

	run git-crypt ls-gpg-users -k prod
	assert_status 0
	assert_eq "Key 'prod' has no GPG collaborators." "$stdout"
}

scenario_respects_repo_state_dir ()
{
	local alice
	alice=$(gpg_gen_key 'Alice' alice@example.com)
	new_encrypted_repo
	git config git-crypt.repoStateDir sub/.git-crypt
	mkdir sub
	git-crypt add-gpg-user --trusted "$alice" >/dev/null 2>&1
	[ -f "sub/.git-crypt/keys/default/0/$alice.gpg" ] || fail "setup: collaborator not in repoStateDir"

	run git-crypt ls-gpg-users
	assert_status 0
	assert_eq "$(printf "Key 'default', version 0:\n    %s Alice <alice@example.com>" "$alice")" "$stdout"
}

scenario_appears_in_help ()
{
	run git-crypt help
	assert_contains "$stdout" "ls-gpg-users"
}
