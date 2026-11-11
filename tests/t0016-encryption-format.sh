# Encryption output for a fixed key is byte-for-byte stable.  The expected
# values were produced by git-crypt 0.8.0 and independently reproduced with
# the openssl command line tool:
#   nonce = first 12 bytes of HMAC-SHA1(hmac_key, plaintext)
#   blob  = "\0GITCRYPT\0" || nonce || AES-256-CTR(aes_key, iv = nonce || 00000000, plaintext)

hex_to_file ()
{
	# hex_to_file FILE HEX...: writes the bytes given as hex digits to FILE
	local hex
	hex=$(printf '%s' "${@:2}")
	printf "$(printf '%s' "$hex" | sed 's/../\\x&/g')" > "$1"
}

file_to_hex ()
{
	od -An -tx1 < "$1" | tr -d ' \n'
}

sha256_hex ()
{
	if command -v sha256sum >/dev/null 2>&1; then
		sha256sum < "$1" | cut -d ' ' -f 1
	else
		shasum -a 256 < "$1" | cut -d ' ' -f 1
	fi
}

# A version 2 key file with aes_key = 00 01 .. 1f and hmac_key = 40 41 .. 7f.
make_fixed_key ()
{
	hex_to_file "$SCENARIO_TMP/key" \
		0047495443525950544b4559 00000002 00000000 \
		00000001 00000004 00000000 \
		00000003 00000020 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f \
		00000005 00000040 404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f \
		00000000
}

scenario_clean_output_matches_known_answers ()
{
	make_fixed_key

	printf 'hi\n' > "$SCENARIO_TMP/plain"
	git-crypt clean --key-file="$SCENARIO_TMP/key" < "$SCENARIO_TMP/plain" > "$SCENARIO_TMP/blob"
	assert_eq 004749544352595054001fe569aecb07ee9fe3f6b4f127979d "$(file_to_hex "$SCENARIO_TMP/blob")" "blob for 'hi\\n'"

	: > "$SCENARIO_TMP/plain"
	git-crypt clean --key-file="$SCENARIO_TMP/key" < "$SCENARIO_TMP/plain" > "$SCENARIO_TMP/blob"
	assert_eq 0047495443525950540066e7d7cd6880730eabda84c1 "$(file_to_hex "$SCENARIO_TMP/blob")" "blob for empty input"

	# 1892 bytes: several AES blocks and more than one 1024-byte I/O buffer
	seq 1 500 > "$SCENARIO_TMP/plain"
	git-crypt clean --key-file="$SCENARIO_TMP/key" < "$SCENARIO_TMP/plain" > "$SCENARIO_TMP/blob"
	assert_eq 00474954435259505400acd5f26123ffbb704ecc2cfc "$(head -c 22 "$SCENARIO_TMP/blob" | od -An -tx1 | tr -d ' \n')" "header of blob for seq 1 500"
	assert_eq 1d9072d666f56543aa1f5aa643d45e1244dfc311b93dd9bc0e55047b8fcecde1 "$(sha256_hex "$SCENARIO_TMP/blob")" "SHA-256 of blob for seq 1 500"
}

scenario_known_blobs_decrypt ()
{
	make_fixed_key
	hex_to_file "$SCENARIO_TMP/blob" 004749544352595054001fe569aecb07ee9fe3f6b4f127979d
	run sh -c "git-crypt smudge --key-file='$SCENARIO_TMP/key' < '$SCENARIO_TMP/blob'"
	assert_status 0
	assert_eq "hi" "$stdout"

	seq 1 500 > "$SCENARIO_TMP/plain"
	git-crypt clean --key-file="$SCENARIO_TMP/key" < "$SCENARIO_TMP/plain" > "$SCENARIO_TMP/blob"
	git-crypt smudge --key-file="$SCENARIO_TMP/key" < "$SCENARIO_TMP/blob" > "$SCENARIO_TMP/out"
	cmp -s "$SCENARIO_TMP/plain" "$SCENARIO_TMP/out" || fail "round trip of seq 1 500"
}
