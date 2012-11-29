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
 */

#include "util.hpp"
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fstream>

int exec_command (const char* command, std::string& output)
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
		output.append(buffer, bytes_read);
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
	int		fd = mkstemp(path);
	if (fd == -1) {
		perror("mkstemp");
		std::exit(9);
	}
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

