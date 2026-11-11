# add-gpg-user with "FPR!" (a specific subkey), and re-adding a user.

# Creates Alice's key with two encryption subkeys and a signing subkey.
# Sets $primary, $enc1, $enc2 and $sign to their fingerprints.
new_key_with_subkeys ()
{
	primary=$(gpg_gen_key 'Alice' alice@example.com) # comes with one encryption subkey
	enc1=$(gpg --batch --with-colons --fingerprint --list-keys "$primary" | awk -F: '/^fpr:/ { n++; if (n == 2) print $10 }')
	enc2=$(gpg_add_subkey "$primary" cv25519 encr)
	sign=$(gpg_add_subkey "$primary" ed25519 sign)
}

# Prints the long key ID that a .gpg file is encrypted to.
recipient_keyid ()
{
	gpg --batch --list-packets "$1" 2>/dev/null | sed -n 's/.*pubkey enc packet:.*keyid \([0-9A-F]*\).*/\1/p'
}

new_repo_with_secret ()
{
	new_encrypted_repo "$@"
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
}

# unlock_clone_with_subkey SUBKEY_FPR: clones origin and tries to unlock it
# with a GNUPGHOME holding only the secret part of the given subkey.
unlock_clone_with_subkey ()
{
	local home
	home=$(new_gnupghome)
	gpg --batch --export-secret-subkeys "$1!" | GNUPGHOME=$home gpg --batch --quiet --import 2>/dev/null
	rm -rf "$SCENARIO_TMP/clone"
	git clone -q "$SCENARIO_TMP/origin" "$SCENARIO_TMP/clone"
	cd "$SCENARIO_TMP/clone"
	GNUPGHOME=$home run git-crypt unlock
	cd "$SCENARIO_TMP"
}

scenario_fpr_bang_encrypts_to_exactly_that_subkey ()
{
	new_key_with_subkeys
	new_repo_with_secret origin
	run git-crypt add-gpg-user --trusted "$enc1!"
	assert_status 0

	local file=".git-crypt/keys/default/0/$primary.gpg"
	[ -f "$file" ] || fail "collaborator file not named by the primary fingerprint"
	assert_eq "${enc1:24}" "$(recipient_keyid "$file")" "recipient key ID"

	unlock_clone_with_subkey "$enc1"
	assert_status 0
	assert_file_content clone/secret.txt 'top secret'

	unlock_clone_with_subkey "$enc2"
	[ "$status" -ne 0 ] || fail "unlocked with the other subkey's secret"
}

scenario_0x_prefixed_and_unprefixed_forms ()
{
	new_key_with_subkeys
	new_repo_with_secret
	run git-crypt add-gpg-user --trusted "0x$enc2!"
	assert_status 0
	assert_eq "${enc2:24}" "$(recipient_keyid ".git-crypt/keys/default/0/$primary.gpg")"

	git-crypt init -k prod >/dev/null 2>&1
	run git-crypt add-gpg-user -k prod --trusted "$enc1!"
	assert_status 0
	assert_eq "${enc1:24}" "$(recipient_keyid ".git-crypt/keys/prod/0/$primary.gpg")"
}

scenario_0x_fpr_bang_is_trusted_without_trusted_flag ()
{
	new_key_with_subkeys
	new_repo_with_secret
	# Alice's key is ultimately trusted in this keyring, so take an untrusted copy of it.
	local home
	home=$(new_gnupghome)
	gpg --batch --export "$primary" | GNUPGHOME=$home gpg --batch --quiet --import 2>/dev/null
	GNUPGHOME=$home run git-crypt add-gpg-user "0x$enc2!"
	assert_status 0
	assert_eq "${enc2:24}" "$(recipient_keyid ".git-crypt/keys/default/0/$primary.gpg")"
}

scenario_sign_only_subkey_is_rejected ()
{
	new_key_with_subkeys
	new_repo_with_secret
	run git-crypt add-gpg-user --trusted "$sign!"
	assert_status 1
	assert_contains "$stderr" "$sign"
	assert_contains "$stderr" "encrypt"
	[ ! -e ".git-crypt/keys/default/0/$primary.gpg" ] || fail "collaborator file written"
}

scenario_unknown_subkey_is_rejected ()
{
	new_key_with_subkeys
	new_repo_with_secret
	run git-crypt add-gpg-user --trusted "0123456789ABCDEF0123456789ABCDEF01234567!"
	assert_status 1
	assert_contains "$stderr" "not found"
}

scenario_readding_a_user_reports_it_and_force_reencrypts ()
{
	new_key_with_subkeys
	new_repo_with_secret
	git-crypt add-gpg-user --trusted "$enc2!" >/dev/null 2>&1
	local file=".git-crypt/keys/default/0/$primary.gpg"
	local head_before
	head_before=$(git rev-parse HEAD)
	assert_eq "${enc2:24}" "$(recipient_keyid "$file")" "setup"

	# Re-adding by primary fingerprint, or by another subkey, is reported and skipped.
	run git-crypt add-gpg-user --trusted "$primary"
	assert_status 0
	assert_contains "$stderr" "already"

	run git-crypt add-gpg-user --trusted "$enc1!"
	assert_status 0
	assert_contains "$stderr" "already"
	assert_contains "$stderr" "--force"
	assert_eq "$head_before" "$(git rev-parse HEAD)" "commit made although nothing changed"
	assert_eq "${enc2:24}" "$(recipient_keyid "$file")" "re-encrypted without --force"

	run git-crypt add-gpg-user --force --trusted "$enc1!"
	assert_status 0
	assert_eq "${enc1:24}" "$(recipient_keyid "$file")" "recipient after --force"
	[ "$head_before" != "$(git rev-parse HEAD)" ] || fail "no commit after --force"
	assert_eq "" "$(git status --porcelain)" "uncommitted changes after --force"
}

scenario_failed_force_reencrypt_keeps_existing_file ()
{
	new_key_with_subkeys
	new_repo_with_secret
	git-crypt add-gpg-user --trusted "$enc1!" >/dev/null 2>&1
	local file=".git-crypt/keys/default/0/$primary.gpg"
	local before
	before=$(file_to_hex_or_sum "$file")

	# Re-adding with --force to a non-encryption subkey fails before gpg runs; make gpg
	# itself fail instead by pointing gpg.program at a wrapper that refuses to encrypt.
	cat > "$SCENARIO_TMP/failing-gpg" <<'GPG'
#!/bin/sh
for arg in "$@"; do [ "$arg" = "-e" ] && { echo "gpg: encryption failed (test)" >&2; exit 2; }; done
exec gpg "$@"
GPG
	chmod +x "$SCENARIO_TMP/failing-gpg"
	git config gpg.program "$SCENARIO_TMP/failing-gpg"

	run git-crypt add-gpg-user --force --trusted "$enc1!"
	assert_status 1
	[ -f "$file" ] || fail "existing collaborator file deleted by failed --force"
	assert_eq "$before" "$(file_to_hex_or_sum "$file")" "collaborator file changed"
	[ ! -e "$file.tmp" ] || fail "temporary file left behind"
	assert_eq "" "$(git status --porcelain)" "working tree changed"
}

file_to_hex_or_sum ()
{
	cksum < "$1"
}
