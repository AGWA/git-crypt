/*
 * Copyright 2012 Andrew Ayer
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

#include "util.hpp"
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fstream>

int exec_command (const char* command, std::ostream& output)
{
	int		pipefd[2];
	if (pipe(pipefd) == -1) {
		perror("pipe");
		std::exit(9);
	}
	pid_t		child = fork();
	if (child == -1) {
		perror("fork");
		std::exit(9);
	}
	if (child == 0) {
		close(pipefd[0]);
		if (pipefd[1] != 1) {
			dup2(pipefd[1], 1);
			close(pipefd[1]);
		}
		execl("/bin/sh", "sh", "-c", command, NULL);
		exit(-1);
	}
	close(pipefd[1]);
	char		buffer[1024];
	ssize_t		bytes_read;
	while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
		output.write(buffer, bytes_read);
	}
	close(pipefd[0]);
	int		status = 0;
	waitpid(child, &status, 0);
	return status;
}

std::string resolve_path (const char* path)
{
	char*		resolved_path_p = realpath(path, NULL);
	std::string	resolved_path(resolved_path_p);
	free(resolved_path_p);
	return resolved_path;
}

void	open_tempfile (std::fstream& file, std::ios_base::openmode mode)
{
	const char*	tmpdir = getenv("TMPDIR");
	size_t		tmpdir_len;
	if (tmpdir) {
		tmpdir_len = strlen(tmpdir);
	} else {
		tmpdir = "/tmp";
		tmpdir_len = 4;
	}
	char*		path = new char[tmpdir_len + 18];
	strcpy(path, tmpdir);
	strcpy(path + tmpdir_len, "/git-crypt.XXXXXX");
	mode_t		old_umask = umask(0077);
	int		fd = mkstemp(path);
	if (fd == -1) {
		perror("mkstemp");
		std::exit(9);
	}
	umask(old_umask);
	file.open(path, mode);
	if (!file.is_open()) {
		perror("open");
		unlink(path);
		std::exit(9);
	}
	unlink(path);
	close(fd);
	delete[] path;
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

