/*
 * Copyright 2012, 2014 Andrew Ayer
 *
 * This file is part of git-crypt.
 *
 * git-crypt is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * git-crypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with git-crypt.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Additional permission under GNU GPL version 3 section 7:
 *
 * If you modify the Program, or any covered work, by linking or
 * combining it with the OpenSSL project's OpenSSL library (or a
 * modified version of that library), containing parts covered by the
 * terms of the OpenSSL or SSLeay licenses, the licensors of the Program
 * grant you additional permission to convey the resulting work.
 * Corresponding Source for a non-source form of such a combination
 * shall include the source code for the parts of OpenSSL used as well
 * as that of the covered work.
 */

#include "commands.hpp"
#include "crypto.hpp"
#include "util.hpp"
#include "key.hpp"
#include "gpg.hpp"
#include "parse_options.hpp"
#include "coprocess.hpp"
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>
#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstddef>
#include <cstring>
#include <cctype>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <exception>
#include <vector>
#include <set>
#include <map>

enum {
	// # of arguments per git checkout call; must be large enough to be efficient but small
	// enough to avoid operating system limits on argument length
	GIT_CHECKOUT_BATCH_SIZE = 100
};

static std::string attribute_name (const char* key_name)
{
	if (key_name) {
		// named key
		return std::string("git-crypt-") + key_name;
	} else {
		// default key
		return "git-crypt";
	}
}

static std::string git_version_string ()
{
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("version");

	std::stringstream		output;
	if (!successful_exit(exec_command(command, output))) {
		throw Error("'git version' failed - is Git installed?");
	}
	std::string			word;
	output >> word; // "git"
	output >> word; // "version"
	output >> word; // "1.7.10.4"
	return word;
}

static std::vector<int> parse_version (const std::string& str)
{
	std::istringstream	in(str);
	std::vector<int>	version;
	std::string		component;
	while (std::getline(in, component, '.')) {
		version.push_back(std::atoi(component.c_str()));
	}
	return version;
}

static const std::vector<int>& git_version ()
{
	static const std::vector<int> version(parse_version(git_version_string()));
	return version;
}

static std::vector<int> make_version (int a, int b, int c)
{
	std::vector<int>	version;
	version.push_back(a);
	version.push_back(b);
	version.push_back(c);
	return version;
}

static void git_config (const std::string& name, const std::string& value)
{
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("config");
	command.push_back(name);
	command.push_back(value);

	if (!successful_exit(exec_command(command))) {
		throw Error("'git config' failed");
	}
}

static bool git_has_config (const std::string& name)
{
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("config");
	command.push_back("--get-all");
	command.push_back(name);

	std::stringstream		output;
	switch (exit_status(exec_command(command, output))) {
		case 0:  return true;
		case 1:  return false;
		default: throw Error("'git config' failed");
	}
}

// Returns the boolean value of the given Git config option, or default_value if it is
// unset or not a valid boolean.
static bool get_git_config_bool (const std::string& name, bool default_value)
{
	// git config --bool --get NAME
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("config");
	command.push_back("--bool");
	command.push_back("--get");
	command.push_back(name);

	std::stringstream		output;
	if (!successful_exit(exec_command(command, output))) {
		return default_value;
	}
	std::string			value;
	std::getline(output, value);
	return value == "true" ? true : value == "false" ? false : default_value;
}

static void git_deconfig (const std::string& name)
{
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("config");
	command.push_back("--remove-section");
	command.push_back(name);

	if (!successful_exit(exec_command(command))) {
		throw Error("'git config' failed");
	}
}

static void configure_git_filters (const char* key_name)
{
	std::string	escaped_git_crypt_path(escape_shell_arg(our_exe_path()));

	if (key_name) {
		// Note: key_name contains only shell-safe characters so it need not be escaped.
		git_config(std::string("filter.git-crypt-") + key_name + ".smudge",
		           escaped_git_crypt_path + " smudge --key-name=" + key_name);
		git_config(std::string("filter.git-crypt-") + key_name + ".clean",
		           escaped_git_crypt_path + " clean --key-name=" + key_name);
		git_config(std::string("filter.git-crypt-") + key_name + ".required", "true");
		git_config(std::string("diff.git-crypt-") + key_name + ".textconv",
		           escaped_git_crypt_path + " diff --key-name=" + key_name);
	} else {
		git_config("filter.git-crypt.smudge", escaped_git_crypt_path + " smudge");
		git_config("filter.git-crypt.clean", escaped_git_crypt_path + " clean");
		git_config("filter.git-crypt.required", "true");
		git_config("diff.git-crypt.textconv", escaped_git_crypt_path + " diff");
	}
}

static void deconfigure_git_filters (const char* key_name)
{
	// deconfigure the git-crypt filters
	if (git_has_config("filter." + attribute_name(key_name) + ".smudge") ||
			git_has_config("filter." + attribute_name(key_name) + ".clean") ||
			git_has_config("filter." + attribute_name(key_name) + ".required")) {

		git_deconfig("filter." + attribute_name(key_name));
	}

	if (git_has_config("diff." + attribute_name(key_name) + ".textconv")) {
		git_deconfig("diff." + attribute_name(key_name));
	}
}

static bool git_checkout_batch (std::vector<std::string>::const_iterator paths_begin, std::vector<std::string>::const_iterator paths_end)
{
	if (paths_begin == paths_end) {
		return true;
	}

	std::vector<std::string>	command;

	command.push_back("git");
	command.push_back("checkout");
	command.push_back("--");

	for (auto path(paths_begin); path != paths_end; ++path) {
		command.push_back(*path);
	}

	if (!successful_exit(exec_command(command))) {
		return false;
	}

	return true;
}

static bool git_checkout (const std::vector<std::string>& paths)
{
	auto paths_begin(paths.begin());
	while (paths.end() - paths_begin >= GIT_CHECKOUT_BATCH_SIZE) {
		if (!git_checkout_batch(paths_begin, paths_begin + GIT_CHECKOUT_BATCH_SIZE)) {
			return false;
		}
		paths_begin += GIT_CHECKOUT_BATCH_SIZE;
	}
	return git_checkout_batch(paths_begin, paths.end());
}

static bool same_key_name (const char* a, const char* b)
{
	return (!a && !b) || (a && b && std::strcmp(a, b) == 0);
}

// "-k default" names the default key, so it must behave exactly as if -k had been
// omitted.  Call this on every key name taken from the command line.
static const char* normalize_key_name (const char* key_name)
{
	return key_name && std::strcmp(key_name, "default") == 0 ? 0 : key_name;
}

static void validate_key_name_or_throw (const char* key_name)
{
	std::string			reason;
	if (!validate_key_name(key_name, &reason)) {
		throw Error(reason);
	}
}

static std::string get_internal_state_path ()
{
	// git rev-parse --git-dir --git-common-dir
	// The internal state lives in the common Git directory so that it's shared
	// by all of the repository's worktrees.
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("rev-parse");
	command.push_back("--git-dir");
	command.push_back("--git-common-dir");

	std::stringstream		output;

	if (!successful_exit(exec_command(command, output))) {
		throw Error("'git rev-parse --git-dir' failed - is this a Git repository?");
	}

	std::string			git_dir;
	std::string			git_common_dir;
	std::getline(output, git_dir);
	std::getline(output, git_common_dir);

	// Git older than 2.5 doesn't know --git-common-dir and echoes it back verbatim.
	// Those versions don't support worktrees, so the Git directory is the common one.
	std::string			path(git_common_dir.empty() || git_common_dir == "--git-common-dir" ? git_dir : git_common_dir);
	path += "/git-crypt";

	return path;
}

static std::string get_internal_keys_path (const std::string& internal_state_path)
{
	return internal_state_path + "/keys";
}

static std::string get_internal_keys_path ()
{
	return get_internal_keys_path(get_internal_state_path());
}

static std::string get_internal_key_path (const char* key_name)
{
	std::string		path(get_internal_keys_path());
	path += "/";
	path += key_name ? key_name : "default";

	return path;
}

std::string get_git_config (const std::string& name)
{
	// git config --get
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("config");
	command.push_back("--get");
	command.push_back(name);

	std::stringstream	output;

	if (!successful_exit(exec_command(command, output))) {
		throw Error("'git config' missing value for key '" + name +"'");
	}

	std::string		value;
	std::getline(output, value);

	return value;
}

static std::string get_repo_state_path ()
{
	// git rev-parse --show-toplevel
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("rev-parse");
	command.push_back("--show-toplevel");

	std::stringstream		output;

	if (!successful_exit(exec_command(command, output))) {
		throw Error("'git rev-parse --show-toplevel' failed - is this a Git repository?");
	}

	std::string			path;
	std::getline(output, path);

	if (path.empty()) {
		// could happen for a bare repo
		throw Error("Could not determine Git working tree - is this a non-bare repo?");
	}

	// Check if the repo state dir has been explicitly configured. If so, use that in path construction.
	if (git_has_config("git-crypt.repoStateDir")) {
		std::string		repoStateDir = get_git_config("git-crypt.repoStateDir");

		// The repoStateDir value must always be relative to git work tree to ensure the repoStateDir can be committed
		// along with the remainder of the repository.
		path += '/' + repoStateDir;
	} else {
		// There is no explicitly configured repo state dir configured, so use the default.
		path += "/.git-crypt";
	}

	return path;
}

static std::string get_repo_keys_path (const std::string& repo_state_path)
{
	return repo_state_path + "/keys";
}

static std::string get_repo_keys_path ()
{
	return get_repo_keys_path(get_repo_state_path());
}

static std::string get_path_to_top ()
{
	// git rev-parse --show-cdup
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("rev-parse");
	command.push_back("--show-cdup");

	std::stringstream		output;

	if (!successful_exit(exec_command(command, output))) {
		throw Error("'git rev-parse --show-cdup' failed - is this a Git repository?");
	}

	std::string			path_to_top;
	std::getline(output, path_to_top);

	return path_to_top;
}

static void get_git_status (std::ostream& output)
{
	// git status -uno --porcelain
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("status");
	command.push_back("-uno"); // don't show untracked files
	command.push_back("--porcelain");

	if (!successful_exit(exec_command(command, output))) {
		throw Error("'git status' failed - is this a Git repository?");
	}
}

static std::string get_current_directory ()
{
	std::vector<char>	buffer(256);
	while (getcwd(&buffer[0], buffer.size()) == nullptr) {
		if (errno != ERANGE) {
			throw System_error("getcwd", "", errno);
		}
		buffer.resize(buffer.size() * 2);
	}
	return &buffer[0];
}

static void change_directory (const std::string& path)
{
	if (chdir(path.c_str()) == -1) {
		throw System_error("chdir", path, errno);
	}
}

// Returns the root directory of every worktree of the repository that's present on disk.
// The unlocked keys and the filter configuration are shared by all worktrees, so lock and
// unlock must act on all of them.  With Git versions too old to have 'git worktree list',
// the current directory is the only worktree.
static std::vector<std::string> get_worktree_paths ()
{
	// git worktree list --porcelain
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("worktree");
	command.push_back("list");
	command.push_back("--porcelain");

	std::stringstream		output;
	std::vector<std::string>	paths;

	if (!successful_exit(exec_command(command, output))) {
		paths.push_back(get_current_directory());
		return paths;
	}

	// Example output (records are separated by blank lines):
	// worktree /path/to/bare-repo
	// bare
	//
	// worktree /path/to/linked-worktree
	// HEAD abcd1234...
	// branch refs/heads/master
	std::string			path;
	bool				bare = false;
	std::string			line;
	bool				more = true;
	while (more) {
		more = static_cast<bool>(std::getline(output, line));
		if (!more || line.empty()) {
			// End of record. Skip bare repositories (no working tree) and
			// worktrees whose directory is gone (nothing to check out there).
			if (!path.empty() && !bare && access(path.c_str(), F_OK) == 0) {
				paths.push_back(path);
			}
			path.clear();
			bare = false;
		} else if (line.compare(0, 9, "worktree ") == 0) {
			path = line.substr(9);
		} else if (line == "bare") {
			bare = true;
		}
	}

	return paths;
}

// Returns the worktrees that have uncommitted changes (ignoring untracked files).
// Running 'git status' also serves as a check that the Git repo is accessible.
static std::vector<std::string> get_unclean_worktrees (const std::vector<std::string>& worktrees)
{
	const std::string		original_directory(get_current_directory());
	std::vector<std::string>	unclean_worktrees;

	for (std::vector<std::string>::const_iterator worktree(worktrees.begin()); worktree != worktrees.end(); ++worktree) {
		change_directory(*worktree);
		std::stringstream	status_output;
		get_git_status(status_output);
		if (status_output.peek() != -1) {
			unclean_worktrees.push_back(*worktree);
		}
	}

	change_directory(original_directory);
	return unclean_worktrees;
}

static void print_unclean_worktrees (const std::vector<std::string>& worktrees, const std::vector<std::string>& unclean_worktrees)
{
	std::clog << "Error: Working directory not clean." << std::endl;
	if (worktrees.size() > 1) {
		std::clog << "The following worktrees have uncommitted changes:" << std::endl;
		for (std::vector<std::string>::const_iterator worktree(unclean_worktrees.begin()); worktree != unclean_worktrees.end(); ++worktree) {
			std::clog << "    " << *worktree << std::endl;
		}
	}
}

// returns filter and diff attributes as a pair
static std::pair<std::string, std::string> get_file_attributes (const std::string& filename)
{
	// git check-attr filter diff -- filename
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("check-attr");
	command.push_back("filter");
	command.push_back("diff");
	command.push_back("--");
	command.push_back(filename);

	std::stringstream		output;
	if (!successful_exit(exec_command(command, output))) {
		throw Error("'git check-attr' failed - is this a Git repository?");
	}

	std::string			filter_attr;
	std::string			diff_attr;

	std::string			line;
	// Example output:
	// filename: filter: git-crypt
	// filename: diff: git-crypt
	while (std::getline(output, line)) {
		// filename might contain ": ", so parse line backwards
		// filename: attr_name: attr_value
		//         ^name_pos  ^value_pos
		const std::string::size_type	value_pos(line.rfind(": "));
		if (value_pos == std::string::npos || value_pos == 0) {
			continue;
		}
		const std::string::size_type	name_pos(line.rfind(": ", value_pos - 1));
		if (name_pos == std::string::npos) {
			continue;
		}

		const std::string		attr_name(line.substr(name_pos + 2, value_pos - (name_pos + 2)));
		const std::string		attr_value(line.substr(value_pos + 2));

		if (attr_value != "unspecified" && attr_value != "unset" && attr_value != "set") {
			if (attr_name == "filter") {
				filter_attr = attr_value;
			} else if (attr_name == "diff") {
				diff_attr = attr_value;
			}
		}
	}

	return std::make_pair(filter_attr, diff_attr);
}

// returns filter and diff attributes as a pair
static std::pair<std::string, std::string> get_file_attributes (const std::string& filename, std::ostream& check_attr_stdin, std::istream& check_attr_stdout)
{
	check_attr_stdin << filename << '\0' << std::flush;

	std::string			filter_attr;
	std::string			diff_attr;

	// Example output:
	// filename\0filter\0git-crypt\0filename\0diff\0git-crypt\0
	for (int i = 0; i < 2; ++i) {
		std::string		filename;
		std::string		attr_name;
		std::string		attr_value;
		std::getline(check_attr_stdout, filename, '\0');
		std::getline(check_attr_stdout, attr_name, '\0');
		std::getline(check_attr_stdout, attr_value, '\0');

		if (attr_value != "unspecified" && attr_value != "unset" && attr_value != "set") {
			if (attr_name == "filter") {
				filter_attr = attr_value;
			} else if (attr_name == "diff") {
				diff_attr = attr_value;
			}
		}
	}

	return std::make_pair(filter_attr, diff_attr);
}

// Looks up the filter and diff attributes of any number of files.  With Git 1.8.5
// (released 27 Nov 2013) and higher, a single `git check-attr --stdin -z` process answers
// for all files.  Older versions don't support -z and --stdin, so a separate `git check-attr`
// process is run for each file.  In a repository with thousands of files, the single
// process is almost 100x faster.
class Attribute_lookup {
	Coprocess		check_attr;
	std::ostream*		check_attr_stdin;
	std::istream*		check_attr_stdout;
public:
	Attribute_lookup ()
	: check_attr_stdin(nullptr), check_attr_stdout(nullptr)
	{
		if (git_version() >= make_version(1, 8, 5)) {
			std::vector<std::string>	command;
			command.push_back("git");
			command.push_back("check-attr");
			command.push_back("--stdin");
			command.push_back("-z");
			command.push_back("filter");
			command.push_back("diff");

			check_attr_stdin = check_attr.stdin_pipe();
			check_attr_stdout = check_attr.stdout_pipe();
			check_attr.spawn(command);
		}
	}

	// returns filter and diff attributes as a pair
	std::pair<std::string, std::string> get (const std::string& filename)
	{
		if (check_attr_stdin) {
			return get_file_attributes(filename, *check_attr_stdin, *check_attr_stdout);
		} else {
			return get_file_attributes(filename);
		}
	}

	// Must be called after the last get()
	void finish ()
	{
		if (check_attr_stdin) {
			check_attr.close_stdin();
			check_attr_stdin = nullptr;
			if (!successful_exit(check_attr.wait())) {
				throw Error("'git check-attr' failed - is this a Git repository?");
			}
		}
	}
};

static bool check_if_blob_is_encrypted (const std::string& object_id)
{
	// git cat-file blob object_id

	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("cat-file");
	command.push_back("blob");
	command.push_back(object_id);

	// TODO: do this more efficiently - don't read entire command output into buffer, only read what we need
	std::stringstream		output;
	if (!successful_exit(exec_command(command, output))) {
		throw Error("'git cat-file' failed - is this a Git repository?");
	}

	char				header[10];
	output.read(header, sizeof(header));
	return output.gcount() == sizeof(header) && std::memcmp(header, "\0GITCRYPT\0", 10) == 0;
}

static bool check_if_file_is_encrypted (const std::string& filename)
{
	// git ls-files -sz filename
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("ls-files");
	command.push_back("-sz");
	command.push_back("--");
	command.push_back(filename);

	std::stringstream		output;
	if (!successful_exit(exec_command(command, output))) {
		throw Error("'git ls-files' failed - is this a Git repository?");
	}

	if (output.peek() == -1) {
		return false;
	}

	std::string			mode;
	std::string			object_id;
	output >> mode >> object_id;

	return check_if_blob_is_encrypted(object_id);
}

static bool is_git_file_mode (const std::string& mode)
{
	return (std::strtoul(mode.c_str(), nullptr, 8) & 0170000) == 0100000;
}

// Which index entries get_tracked_files_with_filter() lists
enum Tracked_files_scope {
	ALL_TRACKED_FILES,		// every regular file in the index
	CHECKED_OUT_TRACKED_FILES	// excluding skip-worktree entries (e.g. outside a sparse checkout)
};

// Lists the regular files in the index (of the current worktree), along with the
// value of their "filter" attribute (empty if unspecified).
static void get_tracked_files_with_filter (std::vector<std::pair<std::string, std::string> >& files, Tracked_files_scope scope)
{
	// git ls-files -ctsz -- path_to_top
	std::vector<std::string>	ls_files_command;
	ls_files_command.push_back("git");
	ls_files_command.push_back("ls-files");
	ls_files_command.push_back("-ctsz");
	ls_files_command.push_back("--");
	const std::string		path_to_top(get_path_to_top());
	if (!path_to_top.empty()) {
		ls_files_command.push_back(path_to_top);
	}

	Coprocess			ls_files;
	std::istream*			ls_files_stdout = ls_files.stdout_pipe();
	ls_files.spawn(ls_files_command);

	Attribute_lookup		attributes;

	while (ls_files_stdout->peek() != -1) {
		std::string		tag;
		std::string		mode;
		std::string		object_id;
		std::string		stage;
		std::string		filename;
		// Example: H 100644 06ec22e5ed0de9280731ef000a10f9c3fbc26338 0\tafile\0
		*ls_files_stdout >> tag >> mode >> object_id >> stage >> std::ws;
		std::getline(*ls_files_stdout, filename, '\0');

		// Tag "S" marks a skip-worktree entry: the file is not checked out (for example
		// because it is outside a sparse checkout), and 'git checkout' must not be given it.
		if (scope == CHECKED_OUT_TRACKED_FILES && tag == "S") {
			continue;
		}

		if (is_git_file_mode(mode)) {
			files.push_back(std::make_pair(filename, attributes.get(filename).first));
		}
	}

	if (!successful_exit(ls_files.wait())) {
		throw Error("'git ls-files' failed - is this a Git repository?");
	}

	attributes.finish();
}

// Lists the checked-out files that are encrypted with the given key.
static void get_encrypted_files (std::vector<std::string>& files, const char* key_name)
{
	std::vector<std::pair<std::string, std::string> >	tracked_files;
	get_tracked_files_with_filter(tracked_files, CHECKED_OUT_TRACKED_FILES);

	const std::string	wanted_attribute(attribute_name(key_name));
	for (std::vector<std::pair<std::string, std::string> >::const_iterator file(tracked_files.begin()); file != tracked_files.end(); ++file) {
		if (file->second == wanted_attribute) {
			files.push_back(file->first);
		}
	}
}

// A key is unlocked when its decrypted key is installed in the internal state
// directory *and* its Git filter is configured.  Anything else (including the
// half-configured state left by an interrupted lock or unlock) counts as locked.
static bool is_key_unlocked (const char* key_name)
{
	return access(get_internal_key_path(key_name).c_str(), F_OK) == 0 &&
	       git_has_config("filter." + attribute_name(key_name) + ".smudge");
}

// Converts a directory entry under a keys directory to a key name ("default" for
// the default key).  Returns false if the entry is not a valid key name.
static bool key_dirent_to_name (const std::string& dirent, std::string& key_name)
{
	if (dirent == "default" || validate_key_name(dirent.c_str())) {
		key_name = dirent;
		return true;
	}
	return false;
}

static void add_key_names_from_directory (std::set<std::string>& key_names, const std::string& path)
{
	if (access(path.c_str(), F_OK) != 0) {
		return;
	}
	const std::vector<std::string>	dirents(get_directory_contents(path.c_str()));
	for (std::vector<std::string>::const_iterator dirent(dirents.begin()); dirent != dirents.end(); ++dirent) {
		std::string	key_name;
		if (key_dirent_to_name(*dirent, key_name)) {
			key_names.insert(key_name);
		}
	}
}

// Adds the key of every git-crypt filter configured in the Git config.
static void add_key_names_from_filter_config (std::set<std::string>& key_names)
{
	// git config --get-regexp '^filter\.git-crypt(-.*)?\.smudge$'
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("config");
	command.push_back("--get-regexp");
	command.push_back("^filter\\.git-crypt(-.*)?\\.smudge$");

	std::stringstream		output;
	const int			status = exit_status(exec_command(command, output));
	if (status != 0 && status != 1) { // 1 means no match
		throw Error("'git config' failed");
	}

	std::string			line;
	while (std::getline(output, line)) {
		// Example line: filter.git-crypt-KEYNAME.smudge "git-crypt" smudge --key-name=KEYNAME
		const std::string	name(line.substr(0, line.find(' ')));
		if (name.size() < 7 + 7) {
			continue;
		}
		const std::string	filter(name.substr(7, name.size() - 7 - 7)); // strip "filter." and ".smudge"
		if (filter == "git-crypt") {
			key_names.insert("default");
		} else if (filter.compare(0, 10, "git-crypt-") == 0 && validate_key_name(filter.c_str() + 10)) {
			key_names.insert(filter.substr(10));
		}
	}
}

// Returns the names of every key the repository uses ("default" for the default key),
// with "default" first and the rest in alphabetical order.  A key is in use if it has
// GPG-encrypted copies committed to the repo state directory, if it is installed in the
// internal state directory, if its filter is configured, or if a tracked file's filter
// attribute refers to it.
static std::vector<std::string> get_repo_key_names ()
{
	std::set<std::string>		key_names;
	add_key_names_from_directory(key_names, get_repo_keys_path());
	add_key_names_from_directory(key_names, get_internal_keys_path());
	add_key_names_from_filter_config(key_names);

	std::vector<std::pair<std::string, std::string> >	tracked_files;
	get_tracked_files_with_filter(tracked_files, ALL_TRACKED_FILES);
	for (std::vector<std::pair<std::string, std::string> >::const_iterator file(tracked_files.begin()); file != tracked_files.end(); ++file) {
		const std::string&	filter(file->second);
		if (filter == "git-crypt") {
			key_names.insert("default");
		} else if (filter.compare(0, 10, "git-crypt-") == 0 && validate_key_name(filter.c_str() + 10)) {
			key_names.insert(filter.substr(10));
		}
	}

	std::vector<std::string>	sorted_key_names;
	if (key_names.erase("default")) {
		sorted_key_names.push_back("default");
	}
	sorted_key_names.insert(sorted_key_names.end(), key_names.begin(), key_names.end());
	return sorted_key_names;
}

// Checks out, in every worktree, the files encrypted with the given keys, so that the
// working tree copies get decrypted or encrypted according to the current filter configuration.
// If verbose_action is non-null (e.g. "Decrypted"), then after a successful checkout, lists
// the checked out files on stdout, grouped by key (and by worktree, if there are several).
static bool checkout_encrypted_files (const std::vector<std::string>& worktrees, const std::vector<const char*>& key_names, const char* verbose_action)
{
	const std::string	original_directory(get_current_directory());
	std::ostringstream	report;

	for (std::vector<std::string>::const_iterator worktree(worktrees.begin()); worktree != worktrees.end(); ++worktree) {
		change_directory(*worktree);

		std::vector<std::string>	encrypted_files;
		for (std::vector<const char*>::const_iterator key_name(key_names.begin()); key_name != key_names.end(); ++key_name) {
			const std::size_t	first_file = encrypted_files.size();
			get_encrypted_files(encrypted_files, *key_name);

			if (verbose_action) {
				report << verbose_action << " with key '" << (*key_name ? *key_name : "default") << "'";
				if (worktrees.size() > 1) {
					report << " in " << *worktree;
				}
				report << ":" << std::endl;
				if (first_file == encrypted_files.size()) {
					report << "    (no files)" << std::endl;
				}
				for (std::size_t i = first_file; i < encrypted_files.size(); ++i) {
					report << "    " << encrypted_files[i] << std::endl;
				}
			}
		}

		// Git won't check out a file if its mtime hasn't changed, so touch every file first.
		for (std::vector<std::string>::const_iterator file(encrypted_files.begin()); file != encrypted_files.end(); ++file) {
			touch_file(*file);
		}
		if (!git_checkout(encrypted_files)) {
			std::clog << "Error: 'git checkout' failed in " << *worktree << std::endl;
			change_directory(original_directory);
			return false;
		}
	}

	change_directory(original_directory);
	std::cout << report.str();
	return true;
}

// Returns the name of a key for use in messages: the key name, or "default".
static std::string key_display_name (const char* key_name)
{
	return key_name ? key_name : "default";
}

// Loads the key from `in`, read from `path`, converting format errors into an Error that
// names the file and the key.
static void load_key_from_stream (Key_file& key_file, std::istream& in, const std::string& path, const char* key_name, bool legacy)
{
	try {
		if (legacy) {
			key_file.load_legacy(in);
		} else {
			key_file.load(in);
		}
	} catch (Key_file::Malformed) {
		throw Error("The key file for key '" + key_display_name(key_name) + "' (" + path + ") is malformed - it may be corrupted.");
	} catch (Key_file::Incompatible) {
		throw Error("The key file for key '" + key_display_name(key_name) + "' (" + path + ") is in an incompatible format - please upgrade git-crypt.");
	}
}

static void load_key (Key_file& key_file, const char* key_name, const char* key_path =0, const char* legacy_path =0)
{
	if (legacy_path) {
		std::ifstream		key_file_in(legacy_path, std::fstream::binary);
		if (!key_file_in) {
			throw Error(std::string("Unable to open key file: ") + legacy_path);
		}
		load_key_from_stream(key_file, key_file_in, legacy_path, key_name, true);
	} else if (key_path) {
		std::ifstream		key_file_in(key_path, std::fstream::binary);
		if (!key_file_in) {
			throw Error(std::string("Unable to open key file: ") + key_path);
		}
		load_key_from_stream(key_file, key_file_in, key_path, key_name, false);
	} else {
		const std::string	internal_key_path(get_internal_key_path(key_name));
		std::ifstream		key_file_in(internal_key_path.c_str(), std::fstream::binary);
		if (!key_file_in) {
			throw Error("Unable to open the key file for key '" + key_display_name(key_name) + "' (" + internal_key_path + ") - have you unlocked/initialized this repository yet?");
		}
		load_key_from_stream(key_file, key_file_in, internal_key_path, key_name, false);
	}
}

static bool decrypt_repo_key (Key_file& key_file, const char* key_name, uint32_t key_version, const std::vector<std::string>& secret_keys, const std::string& keys_path)
{
	std::exception_ptr gpg_error;

	for (std::vector<std::string>::const_iterator seckey(secret_keys.begin()); seckey != secret_keys.end(); ++seckey) {
		std::ostringstream		path_builder;
		path_builder << keys_path << '/' << (key_name ? key_name : "default") << '/' << key_version << '/' << *seckey << ".gpg";
		std::string			path(path_builder.str());
		if (access(path.c_str(), F_OK) == 0) {
			std::stringstream	decrypted_contents;
			try {
				gpg_decrypt_from_file(path, decrypted_contents);
			} catch (const Gpg_error&) {
				gpg_error = std::current_exception();
				continue;
			}
			const std::string	what("The GPG-encrypted key file for key '" + key_display_name(key_name) + "' (" + path + ")");
			const std::string	fix("Ask a collaborator to re-add you with 'git-crypt add-gpg-user --force'.");
			Key_file		this_version_key_file;
			try {
				this_version_key_file.load(decrypted_contents);
			} catch (Key_file::Malformed) {
				throw Error(what + " is malformed - it may be corrupted.\n" + fix);
			} catch (Key_file::Incompatible) {
				throw Error(what + " is in an incompatible format - please upgrade git-crypt.");
			}
			const Key_file::Entry*	this_version_entry = this_version_key_file.get(key_version);
			if (!this_version_entry) {
				throw Error(what + " is malformed because it does not contain the expected key version.\n" + fix);
			}
			if (!same_key_name(key_name, this_version_key_file.get_key_name())) {
				throw Error(what + " is malformed because it does not contain the expected key name.\n" + fix);
			}
			key_file.set_key_name(key_name);
			key_file.add(*this_version_entry);
			return true;
		}
	}

	if (gpg_error) {
		std::rethrow_exception(gpg_error);
	}

	return false;
}

static bool decrypt_repo_keys (std::vector<Key_file>& key_files, uint32_t key_version, const std::vector<std::string>& secret_keys, const std::string& keys_path)
{
	bool				successful = false;
	std::vector<std::string>	dirents;

	if (access(keys_path.c_str(), F_OK) == 0) {
		dirents = get_directory_contents(keys_path.c_str());
	}

	for (std::vector<std::string>::const_iterator dirent(dirents.begin()); dirent != dirents.end(); ++dirent) {
		const char*		key_name = 0;
		if (*dirent != "default") {
			if (!validate_key_name(dirent->c_str())) {
				continue;
			}
			key_name = dirent->c_str();
		}

		Key_file	key_file;
		if (decrypt_repo_key(key_file, key_name, key_version, secret_keys, keys_path)) {
			key_files.push_back(key_file);
			successful = true;
		}
	}
	return successful;
}

// A GPG user to share a key with
struct Gpg_collaborator {
	std::string	fingerprint;	// of the user's primary key; names the collaborator file
	std::string	recipient;	// what to encrypt to: the fingerprint, or "SUBKEY_FINGERPRINT!" to pin a subkey
	bool		trusted;	// encrypt even if GPG doesn't consider the key valid
};

// Encrypts the key to each collaborator, writing <keys_path>/<key_name>/<version>/<FINGERPRINT>.gpg.
// A collaborator whose file already exists is skipped with a message, unless `force` is set,
// in which case the file is re-encrypted.  The paths of the files written are appended to
// new_files, and the collaborators they were written for to written_collabs.
static void encrypt_repo_key (const char* key_name, const Key_file::Entry& key, const std::vector<Gpg_collaborator>& collabs, const std::string& keys_path, bool force, std::vector<std::string>* new_files, std::vector<Gpg_collaborator>* written_collabs)
{
	std::string	key_file_data;
	{
		Key_file this_version_key_file;
		this_version_key_file.set_key_name(key_name);
		this_version_key_file.add(key);
		key_file_data = this_version_key_file.store_to_string();
	}

	for (std::vector<Gpg_collaborator>::const_iterator collab(collabs.begin()); collab != collabs.end(); ++collab) {
		std::ostringstream	path_builder;
		path_builder << keys_path << '/' << (key_name ? key_name : "default") << '/' << key.version << '/' << collab->fingerprint << ".gpg";
		std::string		path(path_builder.str());

		if (access(path.c_str(), F_OK) == 0) {
			if (!force) {
				std::clog << "GPG user " << collab->fingerprint << " has already been added to key '" << (key_name ? key_name : "default") << "' - skipping." << std::endl;
				std::clog << "To re-encrypt the key to this user (e.g. after they changed encryption subkeys), use --force." << std::endl;
				continue;
			}
			remove_file(path);
		}

		mkdir_parent(path);
		try {
			gpg_encrypt_to_file(path, collab->recipient, collab->trusted, key_file_data.data(), key_file_data.size());
		} catch (const Gpg_error&) {
			throw Gpg_error("Failed to encrypt to GPG key " + collab->fingerprint + " (see gpg's message above).\n"
			                "This usually means GPG considers the key unusable: it may have expired or been\n"
			                "revoked, or have no encryption subkey.  Check it with 'gpg --list-keys " + collab->fingerprint + "'.\n"
			                "If it has expired, import an updated copy of the public key from its owner.");
		}
		new_files->push_back(path);
		written_collabs->push_back(*collab);
	}
}

// If user_id has the form [0x]<40 hex digits>!, which selects exactly that primary key or
// subkey, sets fingerprint to the 40 hex digits in upper case and returns true.
static bool parse_exact_key_user_id (const std::string& user_id, std::string& fingerprint)
{
	const std::string::size_type	start = user_id.compare(0, 2, "0x") == 0 ? 2 : 0;
	if (user_id.size() != start + 41 || user_id[user_id.size() - 1] != '!') {
		return false;
	}
	fingerprint = user_id.substr(start, 40);
	if (fingerprint.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
		return false;
	}
	std::transform(fingerprint.begin(), fingerprint.end(), fingerprint.begin(), ::toupper);
	return true;
}

static int parse_plumbing_options (const char** key_name, const char** key_file, int argc, const char** argv)
{
	Options_list	options;
	options.push_back(Option_def("-k", key_name));
	options.push_back(Option_def("--key-name", key_name));
	options.push_back(Option_def("--key-file", key_file));

	int		argi = parse_options(options, argc, argv);
	*key_name = normalize_key_name(*key_name);
	return argi;
}

// Encrypt contents of stdin and write to stdout
int clean (int argc, const char** argv)
{
	const char*		key_name = 0;
	const char*		key_path = 0;
	const char*		legacy_key_path = 0;

	int			argi = parse_plumbing_options(&key_name, &key_path, argc, argv);
	if (argc - argi == 0) {
	} else if (!key_name && !key_path && argc - argi == 1) { // Deprecated - for compatibility with pre-0.4
		legacy_key_path = argv[argi];
	} else {
		std::clog << "Usage: git-crypt clean [--key-name=NAME] [--key-file=PATH]" << std::endl;
		return 2;
	}
	Key_file		key_file;
	load_key(key_file, key_name, key_path, legacy_key_path);

	const Key_file::Entry*	key = key_file.get_latest();
	if (!key) {
		std::clog << "git-crypt: error: key file is empty" << std::endl;
		return 1;
	}

	// Read the entire file

	Hmac_sha1_state	hmac(key->hmac_key, HMAC_KEY_LEN); // Calculate the file's SHA1 HMAC as we go
	uint64_t		file_size = 0;	// Keep track of the length, make sure it doesn't get too big
	std::string		file_contents;	// First 8MB or so of the file go here
	temp_fstream		temp_file;	// The rest of the file spills into a temporary file on disk
	temp_file.exceptions(std::fstream::badbit);

	char			buffer[1024];

	while (std::cin && file_size < Aes_ctr_encryptor::MAX_CRYPT_BYTES) {
		std::cin.read(buffer, sizeof(buffer));

		const size_t	bytes_read = std::cin.gcount();

		hmac.add(reinterpret_cast<unsigned char*>(buffer), bytes_read);
		file_size += bytes_read;

		if (file_size <= 8388608) {
			file_contents.append(buffer, bytes_read);
		} else {
			if (!temp_file.is_open()) {
				temp_file.open(std::fstream::in | std::fstream::out | std::fstream::binary | std::fstream::app);
			}
			temp_file.write(buffer, bytes_read);
		}
	}

	// Make sure the file isn't so large we'll overflow the counter value (which would doom security)
	if (file_size >= Aes_ctr_encryptor::MAX_CRYPT_BYTES) {
		std::clog << "git-crypt: error: file too long to encrypt securely" << std::endl;
		return 1;
	}

	// We use an HMAC of the file as the encryption nonce (IV) for CTR mode.
	// By using a hash of the file we ensure that the encryption is
	// deterministic so git doesn't think the file has changed when it really
	// hasn't.  CTR mode with a synthetic IV is provably semantically secure
	// under deterministic CPA as long as the synthetic IV is derived from a
	// secure PRF applied to the message.  Since HMAC-SHA1 is a secure PRF, this
	// encryption scheme is semantically secure under deterministic CPA.
	// 
	// Informally, consider that if a file changes just a tiny bit, the IV will
	// be completely different, resulting in a completely different ciphertext
	// that leaks no information about the similarities of the plaintexts.  Also,
	// since we're using the output from a secure hash function plus a counter
	// as the input to our block cipher, we should never have a situation where
	// two different plaintext blocks get encrypted with the same CTR value.  A
	// nonce will be reused only if the entire file is the same, which leaks no
	// information except that the files are the same.
	//
	// To prevent an attacker from building a dictionary of hash values and then
	// looking up the nonce (which must be stored in the clear to allow for
	// decryption), we use an HMAC as opposed to a straight hash.

	// Note: Hmac_sha1_state::LEN >= Aes_ctr_encryptor::NONCE_LEN

	unsigned char		digest[Hmac_sha1_state::LEN];
	hmac.get(digest);

	// Write a header that...
	std::cout.write("\0GITCRYPT\0", 10); // ...identifies this as an encrypted file
	std::cout.write(reinterpret_cast<char*>(digest), Aes_ctr_encryptor::NONCE_LEN); // ...includes the nonce

	// Now encrypt the file and write to stdout
	Aes_ctr_encryptor	aes(key->aes_key, digest);

	// First read from the in-memory copy
	const unsigned char*	file_data = reinterpret_cast<const unsigned char*>(file_contents.data());
	size_t			file_data_len = file_contents.size();
	while (file_data_len > 0) {
		const size_t	buffer_len = std::min(sizeof(buffer), file_data_len);
		aes.process(file_data, reinterpret_cast<unsigned char*>(buffer), buffer_len);
		std::cout.write(buffer, buffer_len);
		file_data += buffer_len;
		file_data_len -= buffer_len;
	}

	// Then read from the temporary file if applicable
	if (temp_file.is_open()) {
		temp_file.seekg(0);
		while (temp_file.peek() != -1) {
			temp_file.read(buffer, sizeof(buffer));

			const size_t	buffer_len = temp_file.gcount();

			aes.process(reinterpret_cast<unsigned char*>(buffer),
			            reinterpret_cast<unsigned char*>(buffer),
			            buffer_len);
			std::cout.write(buffer, buffer_len);
		}
	}

	return 0;
}

static int decrypt_file_to_stdout (const Key_file& key_file, const unsigned char* header, std::istream& in)
{
	const unsigned char*	nonce = header + 10;
	uint32_t		key_version = 0; // TODO: get the version from the file header

	const Key_file::Entry*	key = key_file.get(key_version);
	if (!key) {
		std::clog << "git-crypt: error: key version " << key_version << " not available - please unlock with the latest version of the key." << std::endl;
		return 1;
	}

	Aes_ctr_decryptor	aes(key->aes_key, nonce);
	Hmac_sha1_state		hmac(key->hmac_key, HMAC_KEY_LEN);
	while (in) {
		unsigned char	buffer[1024];
		in.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
		aes.process(buffer, buffer, in.gcount());
		hmac.add(buffer, in.gcount());
		std::cout.write(reinterpret_cast<char*>(buffer), in.gcount());
	}

	unsigned char		digest[Hmac_sha1_state::LEN];
	hmac.get(digest);
	if (!leakless_equals(digest, nonce, Aes_ctr_decryptor::NONCE_LEN)) {
		std::clog << "git-crypt: error: encrypted file has been tampered with!" << std::endl;
		// Although we've already written the tampered file to stdout, exiting
		// with a non-zero status will tell git the file has not been filtered,
		// so git will not replace it.
		return 1;
	}

	return 0;
}

enum {
	ENCRYPTED_FILE_HEADER_LEN = 10 + Aes_ctr_decryptor::NONCE_LEN // "\0GITCRYPT\0" followed by the nonce
};

// Copies the rest of `in` to stdout.  Unlike `std::cout << in.rdbuf()`, this never
// puts std::cout into a failed state when `in` has nothing left to read.  (A failed
// std::cout is never flushed, so the output would silently be lost.)
static void copy_to_stdout (std::istream& in)
{
	char			buffer[1024];
	while (in) {
		in.read(buffer, sizeof(buffer));
		std::cout.write(buffer, in.gcount());
	}
}

// Reads the git-crypt header from `in` into `header`, which must hold
// ENCRYPTED_FILE_HEADER_LEN bytes.  Returns true if `in` is a git-crypt encrypted
// file.  Otherwise, writes the input to stdout unchanged - both the bytes consumed
// while probing and the rest of `in` - and returns false.
static bool read_header_or_pass_through (std::istream& in, unsigned char* header)
{
	in.read(reinterpret_cast<char*>(header), ENCRYPTED_FILE_HEADER_LEN);
	const std::streamsize	header_len = in.gcount();
	if (header_len == ENCRYPTED_FILE_HEADER_LEN && std::memcmp(header, "\0GITCRYPT\0", 10) == 0) {
		return true;
	}

	std::cout.write(reinterpret_cast<char*>(header), header_len);
	copy_to_stdout(in);
	return false;
}

// Decrypt contents of stdin and write to stdout
int smudge (int argc, const char** argv)
{
	const char*		key_name = 0;
	const char*		key_path = 0;
	const char*		legacy_key_path = 0;

	int			argi = parse_plumbing_options(&key_name, &key_path, argc, argv);
	if (argc - argi == 0) {
	} else if (!key_name && !key_path && argc - argi == 1) { // Deprecated - for compatibility with pre-0.4
		legacy_key_path = argv[argi];
	} else {
		std::clog << "Usage: git-crypt smudge [--key-name=NAME] [--key-file=PATH]" << std::endl;
		return 2;
	}
	Key_file		key_file;
	load_key(key_file, key_name, key_path, legacy_key_path);

	// Read the header to get the nonce and make sure it's actually encrypted
	unsigned char		header[ENCRYPTED_FILE_HEADER_LEN];
	if (!read_header_or_pass_through(std::cin, header)) {
		// File not encrypted - it has been copied out to stdout as-is.  Commands that walk
		// history (git log -p, blame, rebase) can hit many such blobs, so keep the warning to
		// one line, and let it be turned off with git-crypt.warnUnencrypted=false.
		if (get_git_config_bool("git-crypt.warnUnencrypted", true)) {
			std::clog << "git-crypt: warning: file not encrypted (run 'git-crypt status' for details)" << std::endl;
		}
		return 0;
	}

	return decrypt_file_to_stdout(key_file, header, std::cin);
}

int diff (int argc, const char** argv)
{
	const char*		key_name = 0;
	const char*		key_path = 0;
	const char*		filename = 0;
	const char*		legacy_key_path = 0;

	int			argi = parse_plumbing_options(&key_name, &key_path, argc, argv);
	if (argc - argi == 1) {
		filename = argv[argi];
	} else if (!key_name && !key_path && argc - argi == 2) { // Deprecated - for compatibility with pre-0.4
		legacy_key_path = argv[argi];
		filename = argv[argi + 1];
	} else {
		std::clog << "Usage: git-crypt diff [--key-name=NAME] [--key-file=PATH] FILENAME" << std::endl;
		return 2;
	}
	Key_file		key_file;
	load_key(key_file, key_name, key_path, legacy_key_path);

	// Open the file
	std::ifstream		in(filename, std::fstream::binary);
	if (!in) {
		std::clog << "git-crypt: " << filename << ": unable to open for reading" << std::endl;
		return 1;
	}
	in.exceptions(std::fstream::badbit);

	// Read the header to get the nonce and determine if it's actually encrypted
	unsigned char		header[ENCRYPTED_FILE_HEADER_LEN];
	if (!read_header_or_pass_through(in, header)) {
		// File not encrypted - it has been copied out to stdout as-is
		return 0;
	}

	// Go ahead and decrypt it
	return decrypt_file_to_stdout(key_file, header, in);
}

void help_init (std::ostream& out)
{
	//     |--------------------------------------------------------------------------------| 80 chars
	out << "Usage: git-crypt init [OPTIONS]" << std::endl;
	out << std::endl;
	out << "    -k, --key-name KEYNAME      Initialize the given key, instead of the default" << std::endl;
	out << "    -f, --force                 Generate a new key even though the repository already" << std::endl;
	out << "                                has GPG users or encrypted files for this key" << std::endl;
	out << std::endl;
}

// If the repository (as checked out) already uses the given key - it has GPG-encrypted
// copies of it, or tracked files whose blobs are encrypted with it - returns true and sets
// `evidence` to a description of what was found.
static bool repo_already_uses_key (const char* key_name, std::string& evidence)
{
	const std::string		repo_key_path(get_repo_keys_path() + "/" + key_display_name(key_name));
	if (access(repo_key_path.c_str(), F_OK) == 0) {
		evidence = "it has GPG users in " + repo_key_path;
		return true;
	}

	std::vector<std::string>	files;
	get_encrypted_files(files, key_name);
	for (std::vector<std::string>::const_iterator file(files.begin()); file != files.end(); ++file) {
		if (check_if_file_is_encrypted(*file)) {
			evidence = "it has files encrypted with it, such as " + *file;
			return true;
		}
	}
	return false;
}

int init (int argc, const char** argv)
{
	const char*	key_name = 0;
	bool		force = false;
	Options_list	options;
	options.push_back(Option_def("-k", &key_name));
	options.push_back(Option_def("--key-name", &key_name));
	options.push_back(Option_def("-f", &force));
	options.push_back(Option_def("--force", &force));

	int		argi = parse_options(options, argc, argv);
	key_name = normalize_key_name(key_name);

	if (!key_name && argc - argi == 1) {
		std::clog << "Warning: 'git-crypt init' with a key file is deprecated as of git-crypt 0.4" << std::endl;
		std::clog << "and will be removed in a future release. Please get in the habit of using" << std::endl;
		std::clog << "'git-crypt unlock KEYFILE' instead." << std::endl;
		return unlock(argc, argv);
	}
	if (argc - argi != 0) {
		std::clog << "Error: git-crypt init takes no arguments" << std::endl;
		help_init(std::clog);
		return 2;
	}

	if (key_name) {
		validate_key_name_or_throw(key_name);
	}

	std::string		internal_key_path(get_internal_key_path(key_name));
	if (access(internal_key_path.c_str(), F_OK) == 0) {
		std::clog << "Error: this repository has already been initialized with git-crypt";
		if (key_name) {
			std::clog << " key '" << key_name << "'";
		}
		std::clog << "." << std::endl;
		return 1;
	}

	// A fresh clone of an encrypted repository has no key installed, but generating one
	// would create a new key unrelated to the one its files are encrypted with.
	std::string		evidence;
	if (!force && repo_already_uses_key(key_name, evidence)) {
		const std::string	k_option(key_name ? std::string(" -k ") + key_name : "");
		std::clog << "Error: this repository already uses git-crypt key '" << key_display_name(key_name) << "': " << evidence << "." << std::endl;
		std::clog << "'git-crypt init' would create a new, unrelated key that cannot decrypt those files." << std::endl;
		std::clog << "To decrypt this repository, run 'git-crypt unlock' (with GPG) or 'git-crypt unlock KEY_FILE'." << std::endl;
		std::clog << "To create a new key anyway, run 'git-crypt init" << k_option << " --force'." << std::endl;
		return 1;
	}

	// 1. Generate a key and install it
	std::clog << "Generating key..." << std::endl;
	Key_file		key_file;
	key_file.set_key_name(key_name);
	key_file.generate();

	mkdir_parent(internal_key_path);
	if (!key_file.store_to_file(internal_key_path.c_str())) {
		std::clog << "Error: " << internal_key_path << ": unable to write key file" << std::endl;
		return 1;
	}

	// 2. Configure git for git-crypt
	configure_git_filters(key_name);

	return 0;
}

// Explains why the given file (which failed to load as a key file) is not a valid key file.
static void explain_invalid_key_file (const char* path)
{
	char		header[12] = { 0 };
	std::streamsize	header_len = 0;
	std::streamoff	file_size = -1;
	if (std::strcmp(path, "-") != 0) {
		std::ifstream	in(path, std::fstream::binary);
		in.read(header, sizeof(header));
		header_len = in.gcount();
		in.clear();
		in.seekg(0, std::ios::end);
		file_size = in.tellg();
	}

	if (header_len >= 10 && std::memcmp(header, "\0GITCRYPT\0", 10) == 0) {
		std::clog << "Error: " << path << " is a git-crypt encrypted file, not a key file." << std::endl;
		std::clog << "Specify a key file exported with 'git-crypt export-key'." << std::endl;
	} else if (header_len == 12 && std::memcmp(header, "\0GITCRYPTKEY", 12) == 0) {
		std::clog << "Error: " << path << ": this git-crypt key file is malformed - it may be corrupted." << std::endl;
	} else if (file_size == AES_KEY_LEN + HMAC_KEY_LEN) {
		std::clog << "Error: " << path << ": not a valid git-crypt key file" << std::endl;
		std::clog << "If this key was created prior to git-crypt 0.4, you need to migrate it" << std::endl;
		std::clog << "by running 'git-crypt migrate-key /path/to/old_key /path/to/migrated_key'." << std::endl;
	} else {
		std::clog << "Error: " << path << " is not a git-crypt key file." << std::endl;
		std::clog << "Specify a key file exported with 'git-crypt export-key'." << std::endl;
	}
}

void help_unlock (std::ostream& out)
{
	//     |--------------------------------------------------------------------------------| 80 chars
	out << "Usage: git-crypt unlock [OPTIONS]" << std::endl;
	out << "   or: git-crypt unlock [OPTIONS] KEY_FILE ..." << std::endl;
	out << std::endl;
	out << "    -v, --verbose               List the files that were decrypted" << std::endl;
	out << std::endl;
}
int unlock (int argc, const char** argv)
{
	bool		verbose = false;
	Options_list	options;
	options.push_back(Option_def("-v", &verbose));
	options.push_back(Option_def("--verbose", &verbose));

	const int	argi = parse_options(options, argc, argv);
	argc -= argi;
	argv += argi;

	// 1. Make sure every worktree is clean (ignoring untracked files)
	// We do this because we check out files later, and we don't want the
	// user to lose any changes.  (TODO: only care if encrypted files are
	// modified, since we only check out encrypted files)

	const std::vector<std::string>	worktrees(get_worktree_paths());
	const std::vector<std::string>	unclean_worktrees(get_unclean_worktrees(worktrees));
	if (!unclean_worktrees.empty()) {
		print_unclean_worktrees(worktrees, unclean_worktrees);
		std::clog << "Please commit your changes or 'git stash' them before running 'git-crypt unlock'." << std::endl;
		return 1;
	}

	// 2. Load the key(s)
	std::vector<Key_file>	key_files;
	if (argc > 0) {
		// Read from the symmetric key file(s)

		for (int argi = 0; argi < argc; ++argi) {
			const char*	symmetric_key_file = argv[argi];
			Key_file	key_file;

			try {
				if (std::strcmp(symmetric_key_file, "-") == 0) {
					key_file.load(std::cin);
				} else {
					struct stat	st;
					if (stat(symmetric_key_file, &st) != 0) {
						std::clog << "Error: " << symmetric_key_file << ": " << std::strerror(errno) << std::endl;
						return 1;
					}
					if (S_ISDIR(st.st_mode)) {
						std::clog << "Error: " << symmetric_key_file << " is a directory, not a key file" << std::endl;
						return 1;
					}
					if (!key_file.load_from_file(symmetric_key_file)) {
						std::clog << "Error: " << symmetric_key_file << ": unable to read key file" << std::endl;
						return 1;
					}
				}
			} catch (Key_file::Incompatible) {
				std::clog << "Error: " << symmetric_key_file << " is in an incompatible format" << std::endl;
				std::clog << "Please upgrade to a newer version of git-crypt." << std::endl;
				return 1;
			} catch (Key_file::Malformed) {
				explain_invalid_key_file(symmetric_key_file);
				return 1;
			}

			key_files.push_back(key_file);
		}
	} else {
		// Decrypt GPG key from root of repo
		std::string			repo_keys_path(get_repo_keys_path());
		std::vector<std::string>	gpg_secret_keys(gpg_list_secret_keys());
		// TODO: command-line option to specify the precise secret key to use
		// TODO: don't hard code key version 0 here - instead, determine the most recent version and try to decrypt that, or decrypt all versions if command-line option specified
		// TODO: command line option to only unlock specific key instead of all of them
		// TODO: avoid decrypting repo keys which are already unlocked in the .git directory
		if (!decrypt_repo_keys(key_files, 0, gpg_secret_keys, repo_keys_path)) {
			std::clog << "Error: no GPG secret key available to unlock this repository." << std::endl;
			std::clog << "To unlock with a shared symmetric key instead, specify the path to the symmetric key as an argument to 'git-crypt unlock'." << std::endl;
			std::clog << "To see a list of GPG keys authorized to unlock this repository, run 'git-crypt ls-gpg-users'." << std::endl;
			return 1;
		}
	}


	// 3. Install the key(s) and configure the git filters
	std::vector<const char*>	key_names;
	for (std::vector<Key_file>::iterator key_file(key_files.begin()); key_file != key_files.end(); ++key_file) {
		std::string		internal_key_path(get_internal_key_path(key_file->get_key_name()));
		// TODO: croak if internal_key_path already exists???
		mkdir_parent(internal_key_path);
		if (!key_file->store_to_file(internal_key_path.c_str())) {
			std::clog << "Error: " << internal_key_path << ": unable to write key file" << std::endl;
			std::clog << "To return the repository to a locked state, run 'git-crypt lock -a --force'." << std::endl;
			return 1;
		}

		configure_git_filters(key_file->get_key_name());
		key_names.push_back(key_file->get_key_name());
	}

	// 4. Check out the files that are currently encrypted, in every worktree.
	if (!checkout_encrypted_files(worktrees, key_names, verbose ? "Decrypted" : 0)) {
		std::clog << "git-crypt has been set up but existing encrypted files have not been decrypted" << std::endl;
		std::clog << "(if the key is not the right one for this repository, files fail to decrypt)." << std::endl;
		std::clog << "To return the repository to a locked state, run 'git-crypt lock -a --force'." << std::endl;
		return 1;
	}

	return 0;
}

void help_lock (std::ostream& out)
{
	//     |--------------------------------------------------------------------------------| 80 chars
	out << "Usage: git-crypt lock [OPTIONS]" << std::endl;
	out << std::endl;
	out << "    -a, --all                Lock all keys, instead of just the default" << std::endl;
	out << "    -k, --key-name KEYNAME   Lock the given key, instead of the default" << std::endl;
	out << "    -f, --force              Lock even if unclean (you may lose uncommited work)" << std::endl;
	out << "    --if-unlocked            Do nothing (and succeed) if already locked" << std::endl;
	out << "    -v, --verbose            List the files that were re-encrypted" << std::endl;
	out << std::endl;
}
int lock (int argc, const char** argv)
{
	const char*	key_name = 0;
	bool		all_keys = false;
	bool		force = false;
	bool		if_unlocked = false;
	bool		verbose = false;
	Options_list	options;
	options.push_back(Option_def("-k", &key_name));
	options.push_back(Option_def("--key-name", &key_name));
	options.push_back(Option_def("-a", &all_keys));
	options.push_back(Option_def("--all", &all_keys));
	options.push_back(Option_def("-f", &force));
	options.push_back(Option_def("--force", &force));
	options.push_back(Option_def("--if-unlocked", &if_unlocked));
	options.push_back(Option_def("-v", &verbose));
	options.push_back(Option_def("--verbose", &verbose));

	int			argi = parse_options(options, argc, argv);
	key_name = normalize_key_name(key_name);

	if (argc - argi != 0) {
		std::clog << "Error: git-crypt lock takes no arguments" << std::endl;
		help_lock(std::clog);
		return 2;
	}

	if (all_keys && key_name) {
		std::clog << "Error: -k and --all options are mutually exclusive" << std::endl;
		return 2;
	}

	// 0. With --if-unlocked, do nothing if already locked: no decrypted key is installed
	// for the given key (or, with --all, for any key).  This is the condition under
	// which a plain 'lock' fails with "already locked".  It's checked before the
	// working tree, since uncommitted changes don't matter if nothing is checked out.
	if (if_unlocked) {
		bool		already_locked;
		if (all_keys) {
			const std::string	internal_keys_path(get_internal_keys_path());
			already_locked = access(internal_keys_path.c_str(), F_OK) != 0 ||
			                 get_directory_contents(internal_keys_path.c_str()).empty();
		} else {
			already_locked = access(get_internal_key_path(key_name).c_str(), F_OK) != 0;
		}
		if (already_locked) {
			return 0;
		}
	}

	// 1. Make sure every worktree is clean (ignoring untracked files)
	// We do this because we check out files later, and we don't want the
	// user to lose any changes.  (TODO: only care if encrypted files are
	// modified, since we only check out encrypted files)

	const std::vector<std::string>	worktrees(get_worktree_paths());
	const std::vector<std::string>	unclean_worktrees(get_unclean_worktrees(worktrees));
	if (!force && !unclean_worktrees.empty()) {
		print_unclean_worktrees(worktrees, unclean_worktrees);
		std::clog << "Please commit your changes or 'git stash' them before running 'git-crypt lock'." << std::endl;
		std::clog << "Or, use 'git-crypt lock --force' and possibly lose uncommitted changes." << std::endl;
		return 1;
	}

	// 2. deconfigure the git filters and remove decrypted keys
	std::vector<std::string>	dirents; // owns the strings pointed to by key_names
	std::vector<const char*>	key_names;
	if (all_keys) {
		// deconfigure for all keys
		dirents = get_directory_contents(get_internal_keys_path().c_str());

		for (std::vector<std::string>::const_iterator dirent(dirents.begin()); dirent != dirents.end(); ++dirent) {
			const char* this_key_name = (*dirent == "default" ? 0 : dirent->c_str());
			remove_file(get_internal_key_path(this_key_name));
			deconfigure_git_filters(this_key_name);
			key_names.push_back(this_key_name);
		}
	} else {
		// just handle the given key
		std::string	internal_key_path(get_internal_key_path(key_name));
		if (access(internal_key_path.c_str(), F_OK) == -1 && errno == ENOENT) {
			std::clog << "Error: this repository is already locked";
			if (key_name) {
				std::clog << " with key '" << key_name << "'";
			}
			std::clog << "." << std::endl;
			return 1;
		}

		remove_file(internal_key_path);
		deconfigure_git_filters(key_name);
		key_names.push_back(key_name);
	}

	// 3. Check out the files that are currently decrypted but should be encrypted, in every worktree.
	if (!checkout_encrypted_files(worktrees, key_names, verbose ? "Encrypted" : 0)) {
		std::clog << "git-crypt has been locked up but existing decrypted files have not been encrypted" << std::endl;
		return 1;
	}

	return 0;
}

void help_add_gpg_user (std::ostream& out)
{
	//     |--------------------------------------------------------------------------------| 80 chars
	out << "Usage: git-crypt add-gpg-user [OPTIONS] GPG_USER_ID ..." << std::endl;
	out << std::endl;
	out << "    -k, --key-name KEYNAME      Add GPG user to given key, instead of default" << std::endl;
	out << "    -n, --no-commit             Don't automatically commit" << std::endl;
	out << "    --trusted                   Assume the GPG user IDs are trusted" << std::endl;
	out << "    -f, --force                 Re-encrypt the key to users who were already added" << std::endl;
	out << std::endl;
	out << "To encrypt to a specific subkey, give its full fingerprint followed by '!'." << std::endl;
}
int add_gpg_user (int argc, const char** argv)
{
	const char*		key_name = 0;
	bool			no_commit = false;
	bool			trusted = false;
	bool			force = false;
	Options_list		options;
	options.push_back(Option_def("-k", &key_name));
	options.push_back(Option_def("--key-name", &key_name));
	options.push_back(Option_def("-n", &no_commit));
	options.push_back(Option_def("--no-commit", &no_commit));
	options.push_back(Option_def("--trusted", &trusted));
	options.push_back(Option_def("-f", &force));
	options.push_back(Option_def("--force", &force));

	int			argi = parse_options(options, argc, argv);
	key_name = normalize_key_name(key_name);
	if (argc - argi == 0) {
		std::clog << "Error: no GPG user ID specified" << std::endl;
		help_add_gpg_user(std::clog);
		return 2;
	}

	// build a list of key fingerprints, recipients, and whether the key is trusted, for every collaborator specified on the command line
	std::vector<Gpg_collaborator>	collabs;

	for (int i = argi; i < argc; ++i) {
		Gpg_collaborator	collab;
		std::string		exact_fingerprint;

		if (parse_exact_key_user_id(argv[i], exact_fingerprint)) {
			// FINGERPRINT! - encrypt to exactly this (sub)key, as GnuPG does
			bool		can_encrypt = false;
			if (!gpg_lookup_exact_key(exact_fingerprint, &collab.fingerprint, &can_encrypt)) {
				std::clog << "Error: public key " << exact_fingerprint << " not found in your GPG keyring" << std::endl;
				return 1;
			}
			if (!can_encrypt) {
				std::clog << "Error: GPG key " << exact_fingerprint << " cannot be used to encrypt: it is not an encryption key, or it has expired or been revoked" << std::endl;
				return 1;
			}
			collab.recipient = exact_fingerprint + "!";
		} else {
			std::vector<std::string> keys(gpg_lookup_key(argv[i]));
			if (keys.empty()) {
				std::clog << "Error: public key for '" << argv[i] << "' not found in your GPG keyring" << std::endl;
				return 1;
			}
			if (keys.size() > 1) {
				std::clog << "Error: more than one public key matches '" << argv[i] << "' - please be more specific" << std::endl;
				return 1;
			}
			collab.fingerprint = keys[0];
			collab.recipient = keys[0];
		}

		// A full fingerprint written as 0x<40 hex digits> (optionally followed by '!')
		// identifies the key unambiguously, so it is trusted as if --trusted were given.
		const std::size_t	len = std::strlen(argv[i]);
		const bool		is_full_fingerprint(std::strncmp(argv[i], "0x", 2) == 0 && (len == 42 || (len == 43 && !exact_fingerprint.empty())));
		collab.trusted = trusted || is_full_fingerprint;
		collabs.push_back(collab);
	}

	// TODO: have a retroactive option to grant access to all key versions, not just the most recent
	Key_file			key_file;
	load_key(key_file, key_name);
	const Key_file::Entry*		key = key_file.get_latest();
	if (!key) {
		std::clog << "Error: key file is empty" << std::endl;
		return 1;
	}

	const std::string		state_path(get_repo_state_path());
	const std::string		keys_path(get_repo_keys_path(state_path));
	std::vector<std::string>	new_files;
	std::vector<Gpg_collaborator>	written_collabs;
	std::size_t			nbr_of_readded_collabs = 0;

	for (std::vector<Gpg_collaborator>::const_iterator collab(collabs.begin()); collab != collabs.end(); ++collab) {
		std::ostringstream	path_builder;
		path_builder << keys_path << '/' << (key_name ? key_name : "default") << '/' << key->version << '/' << collab->fingerprint << ".gpg";
		if (force && access(path_builder.str().c_str(), F_OK) == 0) {
			++nbr_of_readded_collabs;
		}
	}

	encrypt_repo_key(key_name, *key, collabs, keys_path, force, &new_files, &written_collabs);

	// Add a .gitatributes file to the repo state directory to prevent files in it from being encrypted.
	const std::string		state_gitattributes_path(state_path + "/.gitattributes");
	if (access(state_gitattributes_path.c_str(), F_OK) != 0) {
		std::ofstream		state_gitattributes_file(state_gitattributes_path.c_str());
		//                          |--------------------------------------------------------------------------------| 80 chars
		state_gitattributes_file << "# Do not edit this file.  To specify the files to encrypt, create your own\n";
		state_gitattributes_file << "# .gitattributes file in the directory where your files are.\n";
		state_gitattributes_file << "* !filter !diff\n";
		state_gitattributes_file << "*.gpg binary\n";
		state_gitattributes_file.close();
		if (!state_gitattributes_file) {
			std::clog << "Error: unable to write " << state_gitattributes_path << std::endl;
			return 1;
		}
		new_files.push_back(state_gitattributes_path);
	}

	// add/commit the new files
	if (!new_files.empty()) {
		// git add NEW_FILE ...
		std::vector<std::string>	command;
		command.push_back("git");
		command.push_back("add");
		command.push_back("--");
		command.insert(command.end(), new_files.begin(), new_files.end());
		if (!successful_exit(exec_command(command))) {
			std::clog << "Error: 'git add' failed" << std::endl;
			return 1;
		}

		// git commit ...
		if (!no_commit) {
			// TODO: include key_name in commit message
			std::ostringstream	commit_message_builder;
			if (written_collabs.empty()) {
				// Only the repo state directory's .gitattributes file is new
				commit_message_builder << "Add git-crypt .gitattributes file\n";
			} else {
				const char*	verb = nbr_of_readded_collabs == 0 ? "Add" :
				                       nbr_of_readded_collabs == written_collabs.size() ? "Re-encrypt key for" : "Add or re-encrypt key for";
				commit_message_builder << verb << " " << written_collabs.size() << " git-crypt collaborator" << (written_collabs.size() != 1 ? "s" : "") << "\n\n" << (nbr_of_readded_collabs == 0 ? "New collaborators" : "Collaborators") << ":\n\n";
				for (std::vector<Gpg_collaborator>::const_iterator collab(written_collabs.begin()); collab != written_collabs.end(); ++collab) {
					commit_message_builder << "    " << collab->fingerprint << '\n';
					commit_message_builder << "        " << gpg_get_uid(collab->fingerprint) << '\n';
				}
			}

			// git commit -m MESSAGE NEW_FILE ...
			command.clear();
			command.push_back("git");
			command.push_back("commit");
			command.push_back("-m");
			command.push_back(commit_message_builder.str());
			command.push_back("--");
			command.insert(command.end(), new_files.begin(), new_files.end());

			if (!successful_exit(exec_command(command))) {
				std::clog << "Error: 'git commit' failed" << std::endl;
				return 1;
			}
		}
	}

	return 0;
}

void help_rm_gpg_user (std::ostream& out)
{
	//     |--------------------------------------------------------------------------------| 80 chars
	out << "Usage: git-crypt rm-gpg-user [OPTIONS] GPG_USER_ID ..." << std::endl;
	out << std::endl;
	out << "    -k, --key-name KEYNAME      Remove user from given key, instead of default" << std::endl;
	out << "    -n, --no-commit             Don't automatically commit" << std::endl;
	out << std::endl;
}
int rm_gpg_user (int argc, const char** argv) // TODO
{
	std::clog << "Error: rm-gpg-user is not yet implemented." << std::endl;
	return 1;
}

void help_ls_gpg_users (std::ostream& out)
{
	//     |--------------------------------------------------------------------------------| 80 chars
	out << "Usage: git-crypt ls-gpg-users [OPTIONS]" << std::endl;
	out << std::endl;
	out << "    -k, --key-name KEYNAME      List users of the given key, instead of all keys" << std::endl;
	out << std::endl;
}

static bool is_all_digits (const std::string& str)
{
	return !str.empty() && str.find_first_not_of("0123456789") == std::string::npos;
}

static bool compare_key_versions (const std::string& a, const std::string& b)
{
	return a.size() != b.size() ? a.size() < b.size() : a < b; // numeric order for all-digit strings
}

int ls_gpg_users (int argc, const char** argv)
{
	const char*		key_name_arg = 0;
	Options_list		options;
	options.push_back(Option_def("-k", &key_name_arg));
	options.push_back(Option_def("--key-name", &key_name_arg));

	int			argi = parse_options(options, argc, argv);
	if (argc - argi != 0) {
		std::clog << "Error: git-crypt ls-gpg-users takes no arguments" << std::endl;
		help_ls_gpg_users(std::clog);
		return 2;
	}
	const char*		key_name = normalize_key_name(key_name_arg);
	if (key_name) {
		validate_key_name_or_throw(key_name);
	}

	// The GPG-encrypted copies of each key are stored as
	// <repo state dir>/keys/<KEYNAME or "default">/<KEY VERSION>/<FINGERPRINT>.gpg
	const std::string		keys_path(get_repo_keys_path());

	std::vector<std::string>	key_dirs;
	if (key_name_arg) {
		key_dirs.push_back(key_name ? key_name : "default");
	} else if (access(keys_path.c_str(), F_OK) == 0) {
		std::set<std::string>	names;
		add_key_names_from_directory(names, keys_path);
		if (names.erase("default")) {
			key_dirs.push_back("default");
		}
		key_dirs.insert(key_dirs.end(), names.begin(), names.end());
	}

	std::map<std::string, std::string>	uids;
	bool					uids_loaded = false;
	bool					any_users = false;

	for (std::vector<std::string>::const_iterator key_dir(key_dirs.begin()); key_dir != key_dirs.end(); ++key_dir) {
		const std::string		key_path(keys_path + "/" + *key_dir);
		if (access(key_path.c_str(), F_OK) != 0) {
			continue;
		}
		std::vector<std::string>	versions(get_directory_contents(key_path.c_str()));
		versions.erase(std::remove_if(versions.begin(), versions.end(), [](const std::string& v) { return !is_all_digits(v); }), versions.end());
		std::sort(versions.begin(), versions.end(), compare_key_versions);

		for (std::vector<std::string>::const_iterator version(versions.begin()); version != versions.end(); ++version) {
			std::vector<std::string>	fingerprints;
			const std::vector<std::string>	dirents(get_directory_contents((key_path + "/" + *version).c_str()));
			for (std::vector<std::string>::const_iterator dirent(dirents.begin()); dirent != dirents.end(); ++dirent) {
				if (dirent->size() > 4 && dirent->compare(dirent->size() - 4, 4, ".gpg") == 0) {
					fingerprints.push_back(dirent->substr(0, dirent->size() - 4));
				}
			}
			if (fingerprints.empty()) {
				continue;
			}
			std::sort(fingerprints.begin(), fingerprints.end());

			if (!uids_loaded) {
				uids = gpg_list_public_key_uids();
				uids_loaded = true;
			}

			std::cout << "Key '" << *key_dir << "', version " << *version << ":" << std::endl;
			for (std::vector<std::string>::const_iterator fingerprint(fingerprints.begin()); fingerprint != fingerprints.end(); ++fingerprint) {
				std::cout << "    " << *fingerprint;
				std::map<std::string, std::string>::const_iterator	uid(uids.find(*fingerprint));
				if (uid == uids.end()) {
					std::cout << " (not in your GPG keyring)";
				} else if (!uid->second.empty()) {
					std::cout << " " << uid->second;
				}
				std::cout << std::endl;
			}
			any_users = true;
		}
	}

	if (!any_users) {
		if (key_name_arg) {
			std::cout << "Key '" << (key_name ? key_name : "default") << "' has no GPG collaborators." << std::endl;
		} else {
			std::cout << "This repository has no GPG collaborators." << std::endl;
		}
	}
	return 0;
}

void help_export_key (std::ostream& out)
{
	//     |--------------------------------------------------------------------------------| 80 chars
	out << "Usage: git-crypt export-key [OPTIONS] FILENAME" << std::endl;
	out << std::endl;
	out << "    -k, --key-name KEYNAME      Export the given key, instead of the default" << std::endl;
	out << std::endl;
	out << "When FILENAME is -, export to standard out." << std::endl;
}
int export_key (int argc, const char** argv)
{
	// TODO: provide options to export only certain key versions
	const char*		key_name = 0;
	Options_list		options;
	options.push_back(Option_def("-k", &key_name));
	options.push_back(Option_def("--key-name", &key_name));

	int			argi = parse_options(options, argc, argv);
	key_name = normalize_key_name(key_name);

	if (argc - argi != 1) {
		std::clog << "Error: no filename specified" << std::endl;
		help_export_key(std::clog);
		return 2;
	}

	Key_file		key_file;
	load_key(key_file, key_name);

	const char*		out_file_name = argv[argi];

	if (std::strcmp(out_file_name, "-") == 0) {
		key_file.store(std::cout);
	} else {
		if (!key_file.store_to_file(out_file_name)) {
			std::clog << "Error: " << out_file_name << ": unable to write key file" << std::endl;
			return 1;
		}
	}

	return 0;
}

void help_keygen (std::ostream& out)
{
	//     |--------------------------------------------------------------------------------| 80 chars
	out << "Usage: git-crypt keygen FILENAME" << std::endl;
	out << std::endl;
	out << "When FILENAME is -, write to standard out." << std::endl;
}
int keygen (int argc, const char** argv)
{
	if (argc != 1) {
		std::clog << "Error: no filename specified" << std::endl;
		help_keygen(std::clog);
		return 2;
	}

	const char*		key_file_name = argv[0];

	if (std::strcmp(key_file_name, "-") != 0 && access(key_file_name, F_OK) == 0) {
		std::clog << key_file_name << ": File already exists" << std::endl;
		return 1;
	}

	std::clog << "Generating key..." << std::endl;
	Key_file		key_file;
	key_file.generate();

	if (std::strcmp(key_file_name, "-") == 0) {
		key_file.store(std::cout);
	} else {
		if (!key_file.store_to_file(key_file_name)) {
			std::clog << "Error: " << key_file_name << ": unable to write key file" << std::endl;
			return 1;
		}
	}
	return 0;
}

void help_migrate_key (std::ostream& out)
{
	//     |--------------------------------------------------------------------------------| 80 chars
	out << "Usage: git-crypt migrate-key OLDFILENAME NEWFILENAME" << std::endl;
	out << std::endl;
	out << "Use - to read from standard in/write to standard out." << std::endl;
}
int migrate_key (int argc, const char** argv)
{
	if (argc != 2) {
		std::clog << "Error: filenames not specified" << std::endl;
		help_migrate_key(std::clog);
		return 2;
	}

	const char*		key_file_name = argv[0];
	const char*		new_key_file_name = argv[1];
	Key_file		key_file;

	try {
		if (std::strcmp(key_file_name, "-") == 0) {
			key_file.load_legacy(std::cin);
		} else {
			std::ifstream	in(key_file_name, std::fstream::binary);
			if (!in) {
				std::clog << "Error: " << key_file_name << ": unable to open for reading" << std::endl;
				return 1;
			}
			key_file.load_legacy(in);
		}

		if (std::strcmp(new_key_file_name, "-") == 0) {
			key_file.store(std::cout);
		} else {
			if (!key_file.store_to_file(new_key_file_name)) {
				std::clog << "Error: " << new_key_file_name << ": unable to write key file" << std::endl;
				return 1;
			}
		}
	} catch (Key_file::Malformed) {
		std::clog << "Error: " << key_file_name << ": not a valid legacy git-crypt key file" << std::endl;
		return 1;
	}

	return 0;
}

void help_refresh (std::ostream& out)
{
	//     |--------------------------------------------------------------------------------| 80 chars
	out << "Usage: git-crypt refresh" << std::endl;
}
int refresh (int argc, const char** argv) // TODO: do a force checkout, much like in unlock
{
	std::clog << "Error: refresh is not yet implemented." << std::endl;
	return 1;
}

void help_status (std::ostream& out)
{
	//     |--------------------------------------------------------------------------------| 80 chars
	out << "Usage: git-crypt status [OPTIONS] [FILE ...]" << std::endl;
	out << "   or: git-crypt status -r [-q]" << std::endl;
	out << "   or: git-crypt status -f [-q]" << std::endl;
	out << std::endl;
	out << "    -e             Show encrypted files only" << std::endl;
	out << "    -u             Show unencrypted files only" << std::endl;
	out << "    -r             Show whether each key is locked or unlocked; exit status" << std::endl;
	out << "                   is 0 if every key is unlocked and 1 if any is locked" << std::endl;
	out << "    -f, --fix      Fix problems with the repository" << std::endl;
	out << "    -q, --quiet    Don't write to standard output (the exit status still" << std::endl;
	out << "                   reports the result)" << std::endl;
	out << "    --no-untracked Don't show untracked files (the default if Git's" << std::endl;
	out << "                   status.showUntrackedFiles option is 'no')" << std::endl;
	//out << "    -z             Machine-parseable output" << std::endl;
	out << std::endl;
}
int status (int argc, const char** argv)
{
	// Usage:
	//  git-crypt status -r [-z]			Show repo status
	//  git-crypt status [-e | -u] [-z] [FILE ...]	Show encrypted status of files
	//  git-crypt status -f				Fix unencrypted blobs

	bool		repo_status_only = false;	// -r show repo status only
	bool		show_encrypted_only = false;	// -e show encrypted files only
	bool		show_unencrypted_only = false;	// -u show unencrypted files only
	bool		fix_problems = false;		// -f fix problems
	bool		machine_output = false;		// -z machine-parseable output
	bool		quiet = false;			// -q don't write to stdout
	bool		no_untracked = false;		// --no-untracked don't show untracked files

	Options_list	options;
	options.push_back(Option_def("-r", &repo_status_only));
	options.push_back(Option_def("-e", &show_encrypted_only));
	options.push_back(Option_def("-u", &show_unencrypted_only));
	options.push_back(Option_def("-f", &fix_problems));
	options.push_back(Option_def("--fix", &fix_problems));
	options.push_back(Option_def("-z", &machine_output));
	options.push_back(Option_def("-q", &quiet));
	options.push_back(Option_def("--quiet", &quiet));
	options.push_back(Option_def("--no-untracked", &no_untracked));

	int		argi = parse_options(options, argc, argv);

	// Everything status reports on stdout goes through `out`, which discards it in quiet mode.
	std::ostream	out(quiet ? nullptr : std::cout.rdbuf());

	if (repo_status_only) {
		if (show_encrypted_only || show_unencrypted_only) {
			std::clog << "Error: -e and -u options cannot be used with -r" << std::endl;
			return 2;
		}
		if (fix_problems) {
			std::clog << "Error: -f option cannot be used with -r" << std::endl;
			return 2;
		}
		if (argc - argi != 0) {
			std::clog << "Error: filenames cannot be specified when -r is used" << std::endl;
			return 2;
		}
	}

	if (show_encrypted_only && show_unencrypted_only) {
		std::clog << "Error: -e and -u options are mutually exclusive" << std::endl;
		return 2;
	}

	if (fix_problems && (show_encrypted_only || show_unencrypted_only)) {
		std::clog << "Error: -e and -u options cannot be used with -f" << std::endl;
		return 2;
	}

	if (machine_output) {
		// TODO: implement machine-parseable output
		std::clog << "Sorry, machine-parseable output is not yet implemented" << std::endl;
		return 2;
	}

	if (repo_status_only) {
		const std::vector<std::string>	key_names(get_repo_key_names());
		if (key_names.empty()) {
			out << "No git-crypt keys found." << std::endl;
			return 0;
		}

		bool				any_locked = false;
		for (std::vector<std::string>::const_iterator key_name(key_names.begin()); key_name != key_names.end(); ++key_name) {
			const bool		unlocked = is_key_unlocked(normalize_key_name(key_name->c_str()));
			out << *key_name << ": " << (unlocked ? "unlocked" : "locked") << std::endl;
			any_locked = any_locked || !unlocked;
		}
		return any_locked ? 1 : 0;
	}

	// Like 'git status', honor status.showUntrackedFiles=no
	if (!no_untracked && git_has_config("status.showUntrackedFiles")) {
		std::string	show_untracked(get_git_config("status.showUntrackedFiles"));
		std::transform(show_untracked.begin(), show_untracked.end(), show_untracked.begin(), ::tolower);
		no_untracked = show_untracked == "no" || show_untracked == "false" || show_untracked == "off" || show_untracked == "0";
	}

	// git ls-files -cotsz --exclude-standard ...  (without -o if untracked files aren't wanted)
	std::vector<std::string>	command;
	command.push_back("git");
	command.push_back("ls-files");
	command.push_back(no_untracked ? "-ctsz" : "-cotsz");
	command.push_back("--exclude-standard");
	command.push_back("--");
	if (argc - argi == 0) {
		const std::string	path_to_top(get_path_to_top());
		if (!path_to_top.empty()) {
			command.push_back(path_to_top);
		}
	} else {
		for (int i = argi; i < argc; ++i) {
			command.push_back(argv[i]);
		}
	}

	std::stringstream		output;
	if (!successful_exit(exec_command(command, output))) {
		throw Error("'git ls-files' failed - is this a Git repository?");
	}

	// Output looks like (w/o newlines):
	// ? .gitignore\0
	// H 100644 06ec22e5ed0de9280731ef000a10f9c3fbc26338 0     afile\0

	std::vector<std::string>	files;
	bool				attribute_errors = false;
	bool				unencrypted_blob_errors = false;
	unsigned int			nbr_of_fixed_blobs = 0;
	unsigned int			nbr_of_fix_errors = 0;
	Attribute_lookup		attributes;
	std::map<std::string, bool>	key_is_unlocked;	// by filter attribute; filled lazily
	std::set<std::string>		locked_keys_of_unencrypted_blobs; // key names ("default" for the default key)

	while (output.peek() != -1) {
		std::string		tag;
		std::string		object_id;
		std::string		filename;
		output >> tag;
		if (tag != "?") {
			std::string	mode;
			std::string	stage;
			output >> mode >> object_id >> stage;
			if (!is_git_file_mode(mode)) {
				continue;
			}
		}
		output >> std::ws;
		std::getline(output, filename, '\0');

		const std::pair<std::string, std::string> file_attrs(attributes.get(filename));

		if (file_attrs.first == "git-crypt" || std::strncmp(file_attrs.first.c_str(), "git-crypt-", 10) == 0) {
			// File is encrypted
			const bool	blob_is_unencrypted = !object_id.empty() && !check_if_blob_is_encrypted(object_id);
			const char*	key_name = file_attrs.first == "git-crypt" ? 0 : file_attrs.first.c_str() + 10;
			bool		key_is_locked = false;
			if (blob_is_unencrypted) {
				// Only an unlocked key can encrypt the file, so a locked key can't be fixed by restaging.
				if (!key_is_unlocked.count(file_attrs.first)) {
					key_is_unlocked[file_attrs.first] = is_key_unlocked(key_name);
				}
				key_is_locked = !key_is_unlocked[file_attrs.first];
				if (key_is_locked) {
					locked_keys_of_unencrypted_blobs.insert(key_name ? key_name : "default");
				}
			}

			if (fix_problems && blob_is_unencrypted && key_is_locked) {
				std::clog << "Error: " << filename << ": cannot stage encrypted version because the repository is locked";
				if (key_name) {
					std::clog << " with key '" << key_name << "'";
				}
				std::clog << " - please run 'git-crypt unlock' first" << std::endl;
				++nbr_of_fix_errors;
			} else if (fix_problems && blob_is_unencrypted) {
				if (access(filename.c_str(), F_OK) != 0) {
					std::clog << "Error: " << filename << ": cannot stage encrypted version because not present in working tree - please 'git rm' or 'git checkout' it" << std::endl;
					++nbr_of_fix_errors;
				} else {
					touch_file(filename);
					std::vector<std::string>	git_add_command;
					git_add_command.push_back("git");
					git_add_command.push_back("add");
					git_add_command.push_back("--");
					git_add_command.push_back(filename);
					if (!successful_exit(exec_command(git_add_command))) {
						throw Error("'git-add' failed");
					}
					if (check_if_file_is_encrypted(filename)) {
						out << filename << ": staged encrypted version" << std::endl;
						++nbr_of_fixed_blobs;
					} else {
						std::clog << "Error: " << filename << ": still unencrypted even after staging" << std::endl;
						++nbr_of_fix_errors;
					}
				}
			} else if (!fix_problems && !show_unencrypted_only) {
				// TODO: output the key name used to encrypt this file
				out << "    encrypted: " << filename;
				if (file_attrs.second != file_attrs.first) {
					// but diff filter is not properly set
					out << " *** WARNING: diff=" << file_attrs.first << " attribute not set ***";
					attribute_errors = true;
				}
				if (blob_is_unencrypted) {
					// File not actually encrypted
					out << " *** WARNING: staged/committed version is NOT ENCRYPTED! ***";
					unencrypted_blob_errors = true;
				}
				out << std::endl;
			}
		} else {
			// File not encrypted
			if (!fix_problems && !show_encrypted_only) {
				out << "not encrypted: " << filename << std::endl;
			}
		}
	}

	attributes.finish();

	int				exit_status = 0;

	if (attribute_errors) {
		out << std::endl;
		out << "Warning: one or more files has a git-crypt filter attribute but not a" << std::endl;
		out << "corresponding git-crypt diff attribute.  For proper 'git diff' operation" << std::endl;
		out << "you should fix the .gitattributes file to specify the correct diff attribute." << std::endl;
		out << "Consult the git-crypt documentation for help." << std::endl;
		exit_status = 1;
	}
	if (unencrypted_blob_errors) {
		out << std::endl;
		out << "Warning: one or more files is marked for encryption via .gitattributes but" << std::endl;
		out << "was staged and/or committed before the .gitattributes file was in effect." << std::endl;
		if (locked_keys_of_unencrypted_blobs.empty()) {
			out << "Run 'git-crypt status' with the '-f' option to stage an encrypted version." << std::endl;
		} else {
			out << "The repository is locked";
			if (locked_keys_of_unencrypted_blobs.size() != 1 || *locked_keys_of_unencrypted_blobs.begin() != "default") {
				out << " with key";
				const char*	separator = locked_keys_of_unencrypted_blobs.size() != 1 ? "s " : " ";
				for (std::set<std::string>::const_iterator key_name(locked_keys_of_unencrypted_blobs.begin()); key_name != locked_keys_of_unencrypted_blobs.end(); ++key_name) {
					out << separator << "'" << *key_name << "'";
					separator = ", ";
				}
			}
			out << ", so these files cannot be encrypted yet." << std::endl;
			out << "Run 'git-crypt unlock' first (if it refuses because of staged changes," << std::endl;
			out << "'git stash' them before unlocking and 'git stash pop' afterwards), then run" << std::endl;
			out << "'git-crypt status' with the '-f' option to stage an encrypted version." << std::endl;
		}
		exit_status = 1;
	}
	if (nbr_of_fixed_blobs) {
		out << "Staged " << nbr_of_fixed_blobs << " encrypted file" << (nbr_of_fixed_blobs != 1 ? "s" : "") << "." << std::endl;
		out << "Warning: if these files were previously committed, unencrypted versions still exist in the repository's history." << std::endl;
	}
	if (nbr_of_fix_errors) {
		out << "Unable to stage " << nbr_of_fix_errors << " file" << (nbr_of_fix_errors != 1 ? "s" : "") << "." << std::endl;
		exit_status = 1;
	}

	return exit_status;
}

