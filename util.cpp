#include "util.hpp"
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

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

