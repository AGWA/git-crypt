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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <utime.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <vector>
#include <string>
#include <cstring>
#include <cstddef>
#include <algorithm>

std::string System_error::message () const
{
	std::string	mesg(action);
	if (!target.empty()) {
		mesg += ": ";
		mesg += target;
	}
	if (error) {
		mesg += ": ";
		mesg += strerror(error);
	}
	return mesg;
}

void	temp_fstream::open (std::ios_base::openmode mode)
{
	close();

	const char*		tmpdir = getenv("TMPDIR");
	size_t			tmpdir_len = tmpdir ? std::strlen(tmpdir) : 0;
	if (tmpdir_len == 0 || tmpdir_len > 4096) {
		// no $TMPDIR or it's excessively long => fall back to /tmp
		tmpdir = "/tmp";
		tmpdir_len = 4;
	}
	std::vector<char>	path_buffer(tmpdir_len + 18);
	char*			path = &path_buffer[0];
	std::strcpy(path, tmpdir);
	std::strcpy(path + tmpdir_len, "/git-crypt.XXXXXX");
	mode_t			old_umask = umask(0077);
	int			fd = mkstemp(path);
	if (fd == -1) {
		int		mkstemp_errno = errno;
		umask(old_umask);
		throw System_error("mkstemp", "", mkstemp_errno);
	}
	umask(old_umask);
	std::fstream::open(path, mode);
	if (!std::fstream::is_open()) {
		unlink(path);
		::close(fd);
		throw System_error("std::fstream::open", path, 0);
	}
	unlink(path);
	::close(fd);
}

void	temp_fstream::close ()
{
	if (std::fstream::is_open()) {
		std::fstream::close();
	}
}

void	mkdir_parent (const std::string& path)
{
	std::string::size_type		slash(path.find('/', 1));
	while (slash != std::string::npos) {
		std::string		prefix(path.substr(0, slash));
		struct stat		status;
		if (stat(prefix.c_str(), &status) == 0) {
			// already exists - make sure it's a directory
			if (!S_ISDIR(status.st_mode)) {
				throw System_error("mkdir_parent", prefix, ENOTDIR);
			}
		} else {
			if (errno != ENOENT) {
				throw System_error("mkdir_parent", prefix, errno);
			}
			// doesn't exist - mkdir it
			if (mkdir(prefix.c_str(), 0777) == -1) {
				throw System_error("mkdir", prefix, errno);
			}
		}

		slash = path.find('/', slash + 1);
	}
}

std::string our_exe_path ()
{
	if (argv0[0] == '/') {
		// argv[0] starts with / => it's an absolute path
		return argv0;
	} else if (std::strchr(argv0, '/')) {
		// argv[0] contains / => it a relative path that should be resolved
		char*		resolved_path_p = realpath(argv0, nullptr);
		std::string	resolved_path(resolved_path_p);
		free(resolved_path_p);
		return resolved_path;
	} else {
		// argv[0] is just a bare filename => not much we can do
		return argv0;
	}
}

int	exit_status (int wait_status)
{
	return wait_status != -1 && WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) : -1;
}

void	touch_file (const std::string& filename)
{
	if (utimes(filename.c_str(), nullptr) == -1 && errno != ENOENT) {
		throw System_error("utimes", filename, errno);
	}
}

void	remove_file (const std::string& filename)
{
	if (unlink(filename.c_str()) == -1 && errno != ENOENT) {
		throw System_error("unlink", filename, errno);
	}
}

static void	init_std_streams_platform ()
{
}

void	create_protected_file (const char* path)
{
	int	fd = open(path, O_WRONLY | O_CREAT, 0600);
	if (fd == -1) {
		throw System_error("open", path, errno);
	}
	close(fd);
}

int util_rename (const char* from, const char* to)
{
	return rename(from, to);
}

std::vector<std::string> get_directory_contents (const char* path)
{
	std::vector<std::string>		contents;

	DIR*					dir = opendir(path);
	if (!dir) {
		throw System_error("opendir", path, errno);
	}
	try {
		errno = 0;
		// Note: readdir is reentrant in new implementations. In old implementations,
		// it might not be, but git-crypt isn't multi-threaded so that's OK.
		// We don't use readdir_r because it's buggy and deprecated:
		//  https://womble.decadent.org.uk/readdir_r-advisory.html
		//  http://austingroupbugs.net/view.php?id=696
		//  http://man7.org/linux/man-pages/man3/readdir_r.3.html
		while (struct dirent* ent = readdir(dir)) {
			if (!(std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)) {
				contents.push_back(ent->d_name);
			}
		}

		if (errno) {
			throw System_error("readdir", path, errno);
		}

	} catch (...) {
		closedir(dir);
		throw;
	}
	closedir(dir);

	std::sort(contents.begin(), contents.end());
	return contents;
}
