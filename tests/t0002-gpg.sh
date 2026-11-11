# Sharing a repository with GPG collaborators.

scenario_add_gpg_user_then_clone_and_unlock ()
{
	local fpr
	fpr=$(gpg_gen_key 'Alice' alice@example.com)

	new_encrypted_repo origin
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
	git-crypt add-gpg-user --trusted "$fpr"
	[ -f ".git-crypt/keys/default/0/$fpr.gpg" ] || fail "collaborator file not committed"
	assert_eq "" "$(git status --porcelain)" "add-gpg-user commits its files"

	cd "$SCENARIO_TMP"
	git clone -q origin clone
	cd clone
	assert_file_encrypted secret.txt
	git-crypt unlock
	assert_file_content secret.txt 'top secret'
}
