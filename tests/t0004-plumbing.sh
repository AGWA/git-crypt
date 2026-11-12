# The smudge / clean / diff plumbing commands invoked by Git.

# Writes the test inputs that are not git-crypt files into $SCENARIO_TMP/in/.
make_plaintext_inputs ()
{
	mkdir -p "$SCENARIO_TMP/in"
	: > "$SCENARIO_TMP/in/empty"
	printf 'x' > "$SCENARIO_TMP/in/1-byte"
	printf 'original\n' > "$SCENARIO_TMP/in/9-bytes"
	printf 'aaaaaaaaaaaaaaaaaaaaa' > "$SCENARIO_TMP/in/21-bytes"
	printf 'aaaaaaaaaaaaaaaaaaaaaa' > "$SCENARIO_TMP/in/22-bytes"
	printf 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n' > "$SCENARIO_TMP/in/long"
	printf '\000GIT' > "$SCENARIO_TMP/in/header-prefix"
	printf '\000GITCRYPT\000abc' > "$SCENARIO_TMP/in/header-short-nonce"
}

scenario_smudge_passes_plaintext_through_byte_for_byte ()
{
	new_encrypted_repo
	make_plaintext_inputs
	for input in "$SCENARIO_TMP"/in/*; do
		run sh -c "git-crypt smudge < '$input' > '$SCENARIO_TMP/out'"
		assert_status 0
		cmp -s "$input" "$SCENARIO_TMP/out" || fail "smudge changed $(basename "$input")"
		assert_contains "$stderr" "not encrypted"
	done
}

scenario_diff_passes_plaintext_through_byte_for_byte ()
{
	new_encrypted_repo
	make_plaintext_inputs
	for input in "$SCENARIO_TMP"/in/*; do
		run sh -c "git-crypt diff '$input' > '$SCENARIO_TMP/out'"
		assert_status 0
		cmp -s "$input" "$SCENARIO_TMP/out" || fail "diff changed $(basename "$input")"
	done
}

scenario_short_plaintext_blob_checks_out_and_diffs ()
{
	new_repo
	git-crypt init >/dev/null 2>&1
	printf 'original\n' > secret.txt
	git add secret.txt
	git commit -q -m 'plaintext before .gitattributes'
	echo 'secret* filter=git-crypt diff=git-crypt' > .gitattributes
	git add .gitattributes
	git commit -q -m 'Add .gitattributes'

	rm secret.txt
	git checkout -- secret.txt 2>/dev/null
	assert_file_content secret.txt 'original'

	assert_contains "$(git show HEAD~1:secret.txt)" 'original'
	run git-crypt diff secret.txt
	assert_eq 'original' "$stdout" "git-crypt diff of a 9-byte plaintext file"
}

scenario_smudge_and_diff_decrypt_encrypted_input ()
{
	new_encrypted_repo
	printf 'top secret\n' > "$SCENARIO_TMP/plain"
	git-crypt clean < "$SCENARIO_TMP/plain" > "$SCENARIO_TMP/enc"

	git-crypt smudge < "$SCENARIO_TMP/enc" > "$SCENARIO_TMP/out"
	cmp -s "$SCENARIO_TMP/plain" "$SCENARIO_TMP/out" || fail "smudge did not decrypt"

	git-crypt diff "$SCENARIO_TMP/enc" > "$SCENARIO_TMP/out"
	cmp -s "$SCENARIO_TMP/plain" "$SCENARIO_TMP/out" || fail "diff did not decrypt"
}

scenario_smudge_rejects_tampered_input ()
{
	new_encrypted_repo
	printf 'top secret\n' | git-crypt clean > "$SCENARIO_TMP/enc"
	# Change the last byte of the ciphertext (to Z, or to Y if it already is Z).
	local size last
	size=$(wc -c < "$SCENARIO_TMP/enc" | tr -d ' ')
	last=$(tail -c 1 "$SCENARIO_TMP/enc" 2>/dev/null)
	head -c $((size - 1)) "$SCENARIO_TMP/enc" > "$SCENARIO_TMP/tampered"
	if [ "$last" = Z ]; then printf 'Y'; else printf 'Z'; fi >> "$SCENARIO_TMP/tampered"
	! cmp -s "$SCENARIO_TMP/enc" "$SCENARIO_TMP/tampered" || fail "tampering left the file unchanged"

	run sh -c "git-crypt smudge < '$SCENARIO_TMP/tampered'"
	assert_status 1
	assert_contains "$stderr" "tampered"

	run git-crypt diff "$SCENARIO_TMP/tampered"
	assert_status 1
	assert_contains "$stderr" "tampered"
}
