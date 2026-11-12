# How the filter and textconv commands refer to the git-crypt binary:
# the bare name "git-crypt" by default, the git-crypt.program option, and
# 'git-crypt refresh-filters' (see docs/adr/0001-*.md).

# Prints the three commands configured for the default key, one per line.
filter_commands ()
{
	git config --get filter.git-crypt.smudge
	git config --get filter.git-crypt.clean
	git config --get diff.git-crypt.textconv
}

expected_commands ()
{
	# expected_commands ESCAPED_PROGRAM
	printf '%s smudge\n%s clean\n%s diff' "$1" "$1" "$1"
}

# Copies the git-crypt binary to DIR/git-crypt and prints that path.
# Note: on Windows, MSYS2 turns a /tmp/... argument into a Windows path
# (D:/.../tmp/...) when it runs git, so tests that compare paths read the
# stored value back with 'git config --get git-crypt.program'.
copy_git_crypt ()
{
	mkdir -p "$1"
	cp "$(command -v git-crypt)" "$1/git-crypt"
	echo "$1/git-crypt"
}

new_repo_with_secret ()
{
	new_encrypted_repo "$@"
	echo 'top secret' > secret.txt
	git add secret.txt
	git commit -q -m 'Add secret'
}

# Makes PATH contain git but not git-crypt, as for a GUI client that
# doesn't inherit the shell's PATH.
use_path_without_git_crypt ()
{
	local git
	git=$(command -v git)
	PATH="$(dirname "$git"):/usr/bin:/bin"
	if command -v git-crypt >/dev/null 2>&1; then
		# git-crypt is installed next to git (e.g. by Homebrew): use a
		# link to git alone.  (Not on Windows, where ln -s copies Git for
		# Windows' launcher, which then refuses to run.)
		export GIT_EXEC_PATH
		GIT_EXEC_PATH=$(git --exec-path)
		mkdir -p "$SCENARIO_TMP/git-only"
		ln -s "$git" "$SCENARIO_TMP/git-only/git"
		PATH="$SCENARIO_TMP/git-only:/usr/bin:/bin"
	fi
	if command -v git-crypt >/dev/null 2>&1; then
		skip "git-crypt is installed in /usr/bin or /bin"
	fi
}

scenario_default_is_bare_name_even_when_invoked_by_absolute_path ()
{
	new_repo
	"$(command -v git-crypt)" init >/dev/null 2>&1
	"$(command -v git-crypt)" init -k prod >/dev/null 2>&1
	assert_eq "$(expected_commands '"git-crypt"')" "$(filter_commands)" "default key commands"
	assert_eq '"git-crypt" smudge --key-name=prod' "$(git config --get filter.git-crypt-prod.smudge)"
	assert_eq '"git-crypt" clean --key-name=prod' "$(git config --get filter.git-crypt-prod.clean)"
	assert_eq '"git-crypt" diff --key-name=prod' "$(git config --get diff.git-crypt-prod.textconv)"
	assert_eq true "$(git config --get filter.git-crypt.required)"
}

scenario_unlock_by_absolute_path_writes_bare_name ()
{
	new_repo_with_secret
	git-crypt export-key "$SCENARIO_TMP/key"
	git-crypt lock
	"$(command -v git-crypt)" unlock "$SCENARIO_TMP/key"
	assert_eq "$(expected_commands '"git-crypt"')" "$(filter_commands)"
	assert_file_content secret.txt 'top secret'
}

scenario_program_option_is_used_and_works_without_path ()
{
	program=$(copy_git_crypt "$SCENARIO_TMP/my bin")
	new_repo
	git config git-crypt.program "$program"
	program=$(git config --get git-crypt.program)
	git-crypt init >/dev/null 2>&1
	assert_eq "$(expected_commands "\"$program\"")" "$(filter_commands)"

	echo 'secret* filter=git-crypt diff=git-crypt' > .gitattributes
	echo 'top secret' > secret.txt
	use_path_without_git_crypt
	git add .gitattributes secret.txt
	git commit -q -m 'Add secret'
	assert_blob_encrypted HEAD:secret.txt
	rm secret.txt
	git checkout -- secret.txt
	assert_file_content secret.txt 'top secret'
}

scenario_bare_name_needs_git_crypt_on_path ()
{
	new_repo_with_secret
	use_path_without_git_crypt
	rm secret.txt
	run git checkout -- secret.txt
	assert_contains "$stderr" "smudge filter git-crypt failed"
}

scenario_program_option_from_global_config ()
{
	program=$(copy_git_crypt "$SCENARIO_TMP/bin")
	export GIT_CONFIG_GLOBAL=$SCENARIO_TMP/gitconfig
	git config --global git-crypt.program "$program"
	program=$(git config --get git-crypt.program)
	new_repo
	git-crypt init >/dev/null 2>&1
	assert_eq "$(expected_commands "\"$program\"")" "$(filter_commands)"
}

scenario_bare_program_name_is_accepted ()
{
	new_repo
	git config git-crypt.program my-git-crypt
	git-crypt init >/dev/null 2>&1
	assert_eq "$(expected_commands '"my-git-crypt"')" "$(filter_commands)"
}

scenario_invalid_program_is_rejected_before_any_change ()
{
	new_repo_with_secret
	git-crypt export-key "$SCENARIO_TMP/key"
	git-crypt lock
	for value in '' 'bin/git-crypt' './git-crypt' '../git-crypt'; do
		git config git-crypt.program "$value"

		run git-crypt init -k other
		assert_status 1
		assert_contains "$stderr" "git-crypt.program"
		[ ! -e .git/git-crypt/keys/other ] || fail "init installed a key with git-crypt.program=[$value]"

		run git-crypt unlock "$SCENARIO_TMP/key"
		assert_status 1
		assert_contains "$stderr" "git-crypt.program"
		[ ! -e .git/git-crypt/keys/default ] || fail "unlock installed a key with git-crypt.program=[$value]"
		run git config --get-regexp '^(filter|diff)\.git-crypt'
		assert_eq "" "$stdout" "filters configured with git-crypt.program=[$value]"
	done
}

scenario_refresh_filters_rejects_invalid_program ()
{
	new_repo_with_secret
	git config git-crypt.program 'bin/git-crypt'
	run git-crypt refresh-filters
	assert_status 1
	assert_contains "$stderr" "git-crypt.program"
	assert_eq "$(expected_commands '"git-crypt"')" "$(filter_commands)"
}

scenario_refresh_filters_repairs_moved_binary ()
{
	old=$(copy_git_crypt "$SCENARIO_TMP/old")
	new_repo
	git config git-crypt.program "$old"
	old=$(git config --get git-crypt.program)
	git-crypt init >/dev/null 2>&1
	echo 'secret* filter=git-crypt diff=git-crypt' > .gitattributes
	echo 'top secret' > secret.txt
	git add .gitattributes secret.txt
	git commit -q -m 'Add secret'

	# git-crypt is upgraded and the old binary is gone.
	git config --unset git-crypt.program
	rm -rf "$SCENARIO_TMP/old"

	run git-crypt refresh-filters
	assert_status 0
	assert_eq "" "$stdout" "stdout"
	assert_contains "$stderr" "key 'default': \"$old\" -> \"git-crypt\""
	assert_contains "$stderr" "git config --global git-crypt.program"
	assert_eq "$(expected_commands '"git-crypt"')" "$(filter_commands)"

	rm secret.txt
	git checkout -- secret.txt
	assert_file_content secret.txt 'top secret'
	assert_eq "+top secret" "$(git show HEAD -- secret.txt | grep '^+top')"
}

scenario_refresh_filters_applies_program_to_all_keys ()
{
	new_repo
	git-crypt init >/dev/null 2>&1
	git-crypt init -k prod >/dev/null 2>&1
	program=$(copy_git_crypt "$SCENARIO_TMP/bin")
	git config git-crypt.program "$program"
	program=$(git config --get git-crypt.program)

	run git-crypt refresh-filters
	assert_status 0
	assert_contains "$stderr" "key 'default': \"git-crypt\" -> \"$program\""
	assert_contains "$stderr" "key 'prod': \"git-crypt\" -> \"$program\""
	assert_not_contains "$stderr" "git config --global git-crypt.program"
	assert_eq "$(expected_commands "\"$program\"")" "$(filter_commands)"
	assert_eq "\"$program\" smudge --key-name=prod" "$(git config --get filter.git-crypt-prod.smudge)"
	assert_eq "\"$program\" clean --key-name=prod" "$(git config --get filter.git-crypt-prod.clean)"
	assert_eq "\"$program\" diff --key-name=prod" "$(git config --get diff.git-crypt-prod.textconv)"
}

scenario_refresh_filters_is_silent_when_up_to_date ()
{
	new_repo_with_secret
	run git-crypt refresh-filters
	assert_status 0
	assert_eq "" "$stdout$stderr" "output"
	assert_eq "$(expected_commands '"git-crypt"')" "$(filter_commands)"
}

scenario_refresh_filters_does_nothing_in_locked_repo ()
{
	new_repo_with_secret
	git-crypt lock
	run git-crypt refresh-filters
	assert_status 0
	assert_eq "" "$stdout$stderr" "output"
	run git config --get-regexp '^(filter|diff)\.git-crypt'
	assert_eq "" "$stdout" "filters configured in a locked repo"
	assert_file_encrypted secret.txt
}

scenario_refresh_filters_takes_no_arguments ()
{
	new_repo_with_secret
	run git-crypt refresh-filters -k default
	assert_status 2
	run git-crypt refresh-filters extra
	assert_status 2
	assert_contains "$stderr" "refresh-filters takes no arguments"
}

scenario_refresh_filters_outside_repository_fails ()
{
	mkdir not-a-repo
	cd not-a-repo
	export GIT_CEILING_DIRECTORIES=$SCENARIO_TMP
	run git-crypt refresh-filters
	assert_status 1
	assert_contains "$stderr" "Git repository"
}

scenario_unlock_rewrites_absolute_path ()
{
	old=$(copy_git_crypt "$SCENARIO_TMP/old")
	export GIT_CONFIG_GLOBAL=$SCENARIO_TMP/gitconfig
	git config --global git-crypt.program "$old"
	old=$(git config --get git-crypt.program)
	new_repo_with_secret
	git-crypt export-key "$SCENARIO_TMP/key"
	git config --global --unset git-crypt.program

	run git-crypt unlock "$SCENARIO_TMP/key"
	assert_status 0
	assert_contains "$stderr" "key 'default': \"$old\" -> \"git-crypt\""
	assert_contains "$stderr" "git config --global git-crypt.program"
	assert_eq "$(expected_commands '"git-crypt"')" "$(filter_commands)"
}

# Unlock checks that the worktree is clean, which runs the clean filter, so it
# can't repair commands whose binary is gone; refresh-filters can.
scenario_unlock_cannot_repair_missing_binary ()
{
	old=$(copy_git_crypt "$SCENARIO_TMP/old")
	export GIT_CONFIG_GLOBAL=$SCENARIO_TMP/gitconfig
	git config --global git-crypt.program "$old"
	new_repo_with_secret
	git-crypt export-key "$SCENARIO_TMP/key"
	git config --global --unset git-crypt.program
	rm -rf "$SCENARIO_TMP/old"
	touch secret.txt # make Git re-run the clean filter

	run git-crypt unlock "$SCENARIO_TMP/key"
	assert_status 1
	assert_contains "$stderr" "clean filter 'git-crypt' failed"
	git-crypt refresh-filters 2>/dev/null
	git-crypt unlock "$SCENARIO_TMP/key"
	assert_file_content secret.txt 'top secret'
}

scenario_help_describes_refresh_filters ()
{
	run git-crypt help refresh-filters
	assert_status 0
	assert_contains "$stdout" "Usage: git-crypt refresh-filters"
	run git-crypt help
	assert_contains "$stdout" "refresh-filters"
}

scenario_refresh_filters_ignores_invalid_program_in_locked_repo ()
{
	new_repo_with_secret
	git-crypt lock
	git config git-crypt.program 'bin/git-crypt'
	run git-crypt refresh-filters
	assert_status 0
	assert_eq "" "$stdout$stderr" "output"
}

scenario_global_program_then_refresh_fixes_gui_setup ()
{
	program=$(copy_git_crypt "$SCENARIO_TMP/bin")
	new_repo_with_secret
	export GIT_CONFIG_GLOBAL=$SCENARIO_TMP/gitconfig
	git config --global git-crypt.program "$program"
	git-crypt refresh-filters 2>/dev/null
	use_path_without_git_crypt
	rm secret.txt
	git checkout -- secret.txt
	assert_file_content secret.txt 'top secret'
}
