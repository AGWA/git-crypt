# lock and unlock in sparse checkouts must only touch files in the sparse set.

# An origin repository with two encrypted files in different directories,
# and its key exported to $SCENARIO_TMP/key.
new_origin_with_two_dirs ()
{
	new_repo origin
	git-crypt init >/dev/null 2>&1
	echo '*.txt filter=git-crypt diff=git-crypt' > .gitattributes
	mkdir foo bar
	echo 'foo secret' > foo/foo.txt
	echo 'bar secret' > bar/bar.txt
	git add .
	git commit -q -m 'Add files'
	git-crypt export-key "$SCENARIO_TMP/key"
	cd "$SCENARIO_TMP"
}

# sparse_clone MODE: clones origin into ./clone with only foo/ checked out.
sparse_clone ()
{
	git clone -q --no-checkout origin clone
	cd clone
	if [ "$1" = cone ]; then
		git sparse-checkout set --cone foo
	else
		# Patterns on stdin, not as arguments: on Windows, MSYS2 would turn
		# arguments like /foo/ into Windows paths.
		printf '%s\n' '/*' '!/*/' '/foo/' | git sparse-checkout set --no-cone --stdin
	fi
	git checkout -q main 2>/dev/null
	[ -f foo/foo.txt ] || fail "sparse checkout setup: foo/foo.txt missing"
	[ ! -e bar/bar.txt ] || fail "sparse checkout setup: bar/bar.txt present"
}

check_unlock_and_lock ()
{
	run git-crypt unlock "$SCENARIO_TMP/key"
	assert_status 0
	assert_file_content foo/foo.txt 'foo secret'
	[ ! -e bar/bar.txt ] || fail "bar/bar.txt materialized by unlock"

	run git-crypt lock
	assert_status 0
	assert_file_encrypted foo/foo.txt
	[ ! -e bar/bar.txt ] || fail "bar/bar.txt materialized by lock"
}

scenario_cone_mode ()
{
	new_origin_with_two_dirs
	sparse_clone cone
	check_unlock_and_lock
}

scenario_non_cone_mode ()
{
	new_origin_with_two_dirs
	sparse_clone no-cone
	check_unlock_and_lock
}

scenario_sparse_linked_worktree ()
{
	new_origin_with_two_dirs
	git clone -q origin clone
	cd clone
	git worktree add -q --no-checkout "$SCENARIO_TMP/wt" -b other
	(
		cd "$SCENARIO_TMP/wt"
		git sparse-checkout set --cone foo
		git checkout -q other 2>/dev/null
	)
	[ ! -e "$SCENARIO_TMP/wt/bar/bar.txt" ] || fail "setup: bar present in worktree"

	run git-crypt unlock "$SCENARIO_TMP/key"
	assert_status 0
	assert_file_content bar/bar.txt 'bar secret'
	assert_file_content "$SCENARIO_TMP/wt/foo/foo.txt" 'foo secret'
	[ ! -e "$SCENARIO_TMP/wt/bar/bar.txt" ] || fail "bar/bar.txt materialized in worktree by unlock"

	run git-crypt lock
	assert_status 0
	assert_file_encrypted bar/bar.txt
	assert_file_encrypted "$SCENARIO_TMP/wt/foo/foo.txt"
	[ ! -e "$SCENARIO_TMP/wt/bar/bar.txt" ] || fail "bar/bar.txt materialized in worktree by lock"
}
