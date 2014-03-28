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

#include "git-crypt.hpp"
#include "util.hpp"
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fstream>

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

std::string readlink (const char* pathname)
{
	std::vector<char>	buffer(64);
	ssize_t			len;

	while ((len = ::readlink(pathname, &buffer[0], buffer.size())) == static_cast<ssize_t>(buffer.size())) {
		// buffer may have been truncated - grow and try again
		buffer.resize(buffer.size() * 2);
	}
	if (len == -1) {
		throw System_error("readlink", pathname, errno);
	}

	return std::string(buffer.begin(), buffer.begin() + len);
}

std::string our_exe_path ()
{
	try {
		return readlink("/proc/self/exe");
	} catch (const System_error&) {
		if (argv0[0] == '/') {
			// argv[0] starts with / => it's an absolute path
			return argv0;
		} else if (std::strchr(argv0, '/')) {
			// argv[0] contains / => it a relative path that should be resolved
			char*		resolved_path_p = realpath(argv0, NULL);
			std::string	resolved_path(resolved_path_p);
			free(resolved_path_p);
			return resolved_path;
		} else {
			// argv[0] is just a bare filename => not much we can do
			return argv0;
		}
	}
}

int exec_command (const char* command, std::ostream& output)
{
	int		pipefd[2];
	if (pipe(pipefd) == -1) {
		throw System_error("pipe", "", errno);
	}
	pid_t		child = fork();
	if (child == -1) {
		int	fork_errno = errno;
		close(pipefd[0]);
		close(pipefd[1]);
		throw System_error("fork", "", fork_errno);
	}
	if (child == 0) {
		close(pipefd[0]);
		if (pipefd[1] != 1) {
			dup2(pipefd[1], 1);
			close(pipefd[1]);
		}
		execl("/bin/sh", "sh", "-c", command, NULL);
		perror("/bin/sh");
		_exit(-1);
	}
	close(pipefd[1]);
	char		buffer[1024];
	ssize_t		bytes_read;
	while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
		output.write(buffer, bytes_read);
	}
	if (bytes_read == -1) {
		int	read_errno = errno;
		close(pipefd[0]);
		throw System_error("read", "", read_errno);
	}
	close(pipefd[0]);
	int		status = 0;
	if (waitpid(child, &status, 0) == -1) {
		throw System_error("waitpid", "", errno);
	}
	return status;
}

int exec_command_with_input (const char* command, const char* p, size_t len)
{
	int		pipefd[2];
	if (pipe(pipefd) == -1) {
		throw System_error("pipe", "", errno);
	}
	pid_t		child = fork();
	if (child == -1) {
		int	fork_errno = errno;
		close(pipefd[0]);
		close(pipefd[1]);
		throw System_error("fork", "", fork_errno);
	}
	if (child == 0) {
		close(pipefd[1]);
		if (pipefd[0] != 0) {
			dup2(pipefd[0], 0);
			close(pipefd[0]);
		}
		execl("/bin/sh", "sh", "-c", command, NULL);
		perror("/bin/sh");
		_exit(-1);
	}
	close(pipefd[0]);
	while (len > 0) {
		ssize_t	bytes_written = write(pipefd[1], p, len);
		if (bytes_written == -1) {
			int	write_errno = errno;
			close(pipefd[1]);
			throw System_error("write", "", write_errno);
		}
		p += bytes_written;
		len -= bytes_written;
	}
	close(pipefd[1]);
	int		status = 0;
	if (waitpid(child, &status, 0) == -1) {
		throw System_error("waitpid", "", errno);
	}
	return status;
}

bool successful_exit (int status)
{
	return status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

void	open_tempfile (std::fstream& file, std::ios_base::openmode mode)
{
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
	file.open(path, mode);
	if (!file.is_open()) {
		unlink(path);
		close(fd);
		throw System_error("std::fstream::open", path, 0);
	}
	unlink(path);
	close(fd);
}

std::string	escape_shell_arg (const std::string& str)
{
	std::string	new_str;
	new_str.push_back('"');
	for (std::string::const_iterator it(str.begin()); it != str.end(); ++it) {
		if (*it == '"' || *it == '\\' || *it == '$' || *it == '`') {
			new_str.push_back('\\');
		}
		new_str.push_back(*it);
	}
	new_str.push_back('"');
	return new_str;
}

uint32_t	load_be32 (const unsigned char* p)
{
	return (static_cast<uint32_t>(p[3]) << 0) |
	       (static_cast<uint32_t>(p[2]) << 8) |
	       (static_cast<uint32_t>(p[1]) << 16) |
	       (static_cast<uint32_t>(p[0]) << 24);
}

void		store_be32 (unsigned char* p, uint32_t i)
{
	p[3] = i; i >>= 8;
	p[2] = i; i >>= 8;
	p[1] = i; i >>= 8;
	p[0] = i;
}

bool		read_be32 (std::istream& in, uint32_t& i)
{
	unsigned char buffer[4];
	in.read(reinterpret_cast<char*>(buffer), 4);
	if (in.gcount() != 4) {
		return false;
	}
	i = load_be32(buffer);
	return true;
}

void		write_be32 (std::ostream& out, uint32_t i)
{
	unsigned char buffer[4];
	store_be32(buffer, i);
	out.write(reinterpret_cast<const char*>(buffer), 4);
}

