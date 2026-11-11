# The smudge / clean / diff plumbing commands invoked by Git.

# Known bug (#2): on libstdc++ (Linux) non-git-crypt input shorter than the
# 22-byte header comes out empty.  macOS (libc++) is unaffected.
xfail_smudge_passes_short_plaintext_through ()
{
	new_encrypted_repo
	run sh -c "printf 'original\n' | git-crypt smudge"
	assert_status 0
	assert_eq "original" "$stdout" "smudge output"
}
