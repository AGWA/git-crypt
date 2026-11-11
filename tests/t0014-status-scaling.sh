# git-crypt status with many files: a constant number of git processes, and
# the option to skip untracked files.

# Puts a "git" wrapper first in PATH that counts its invocations in
# $SCENARIO_TMP/git-count.
count_git_processes ()
{
	local real_git
	real_git=$(command -v git)
	mkdir -p "$SCENARIO_TMP/shim"
	cat > "$SCENARIO_TMP/shim/git" <<SHIM
#!/bin/sh
echo x >> "$SCENARIO_TMP/git-count"
exec "$real_git" "\$@"
SHIM
	chmod +x "$SCENARIO_TMP/shim/git"
	: > "$SCENARIO_TMP/git-count"
	PATH="$SCENARIO_TMP/shim:$PATH"
}

git_process_count ()
{
	wc -l < "$SCENARIO_TMP/git-count" | tr -d ' '
}

new_repo_with_untracked_files ()
{
	new_encrypted_repo
	echo 'top secret' > secret.txt
	echo 'public' > public.txt
	git add secret.txt public.txt
	git commit -q -m 'Add files'
	mkdir untracked
	(cd untracked && seq 1 "$1" | sed 's/^/file/' | xargs touch)
}

scenario_status_spawns_constant_number_of_git_processes ()
{
	new_repo_with_untracked_files 10000
	count_git_processes
	run git-crypt status
	assert_status 0
	assert_contains "$stdout" "    encrypted: secret.txt"
	assert_contains "$stdout" "not encrypted: untracked/file9999"
	[ "$(git_process_count)" -le 10 ] || fail "status spawned $(git_process_count) git processes"
}

scenario_status_skips_untracked_files_when_showuntrackedfiles_is_no ()
{
	new_repo_with_untracked_files 3
	git config status.showUntrackedFiles no
	run git-crypt status
	assert_status 0
	assert_eq "$(printf 'not encrypted: .gitattributes\nnot encrypted: public.txt\n    encrypted: secret.txt')" "$stdout"
}

scenario_status_no_untracked_flag ()
{
	new_repo_with_untracked_files 3
	run git-crypt status --no-untracked
	assert_status 0
	assert_eq "$(printf 'not encrypted: .gitattributes\nnot encrypted: public.txt\n    encrypted: secret.txt')" "$stdout"

	run git-crypt status
	assert_contains "$stdout" "not encrypted: untracked/file1"
}

scenario_status_in_linked_worktree ()
{
	new_repo_with_untracked_files 3
	git worktree add -q "$SCENARIO_TMP/wt" -b other
	cd "$SCENARIO_TMP/wt"
	echo 'untracked secret' > secret-new
	mkdir untracked-wt
	(cd untracked-wt && seq 1 50 | sed 's/^/file/' | xargs touch)
	count_git_processes
	run git-crypt status
	assert_status 0
	assert_contains "$stdout" "$(printf "    encrypted: secret-new\nnot encrypted: untracked-wt/file1\n")"
	assert_contains "$stdout" "$(printf "not encrypted: untracked-wt/file9\nnot encrypted: .gitattributes\nnot encrypted: public.txt\n    encrypted: secret.txt")"
	[ "$(git_process_count)" -le 10 ] || fail "status spawned $(git_process_count) git processes"
}
