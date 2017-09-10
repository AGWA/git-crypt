/*
 * Copyright 2015 Andrew Ayer
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

#include "coprocess.hpp"
#include "util.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

static int execvp (const std::string& file, const std::vector<std::string>& args)
{
	std::vector<const char*>	args_c_str;
	args_c_str.reserve(args.size());
	for (std::vector<std::string>::const_iterator arg(args.begin()); arg != args.end(); ++arg) {
		args_c_str.push_back(arg->c_str());
	}
	args_c_str.push_back(nullptr);
	return execvp(file.c_str(), const_cast<char**>(&args_c_str[0]));
}

Coprocess::Coprocess ()
{
	pid = -1;
	stdin_pipe_reader = -1;
	stdin_pipe_writer = -1;
	stdin_pipe_ostream = nullptr;
	stdout_pipe_reader = -1;
	stdout_pipe_writer = -1;
	stdout_pipe_istream = nullptr;
}

Coprocess::~Coprocess ()
{
	close_stdin();
	close_stdout();
}

std::ostream*	Coprocess::stdin_pipe ()
{
	if (!stdin_pipe_ostream) {
		int	fds[2];
		if (pipe(fds) == -1) {
			throw System_error("pipe", "", errno);
		}
		stdin_pipe_reader = fds[0];
		stdin_pipe_writer = fds[1];
		stdin_pipe_ostream = new ofhstream(this, write_stdin);
	}
	return stdin_pipe_ostream;
}

void		Coprocess::close_stdin ()
{
	delete stdin_pipe_ostream;
	stdin_pipe_ostream = nullptr;
	if (stdin_pipe_writer != -1) {
		close(stdin_pipe_writer);
		stdin_pipe_writer = -1;
	}
	if (stdin_pipe_reader != -1) {
		close(stdin_pipe_reader);
		stdin_pipe_reader = -1;
	}
}

std::istream*	Coprocess::stdout_pipe ()
{
	if (!stdout_pipe_istream) {
		int	fds[2];
		if (pipe(fds) == -1) {
			throw System_error("pipe", "", errno);
		}
		stdout_pipe_reader = fds[0];
		stdout_pipe_writer = fds[1];
		stdout_pipe_istream = new ifhstream(this, read_stdout);
	}
	return stdout_pipe_istream;
}

void		Coprocess::close_stdout ()
{
	delete stdout_pipe_istream;
	stdout_pipe_istream = nullptr;
	if (stdout_pipe_writer != -1) {
		close(stdout_pipe_writer);
		stdout_pipe_writer = -1;
	}
	if (stdout_pipe_reader != -1) {
		close(stdout_pipe_reader);
		stdout_pipe_reader = -1;
	}
}

void		Coprocess::spawn (const std::vector<std::string>& args)
{
	pid = fork();
	if (pid == -1) {
		throw System_error("fork", "", errno);
	}
	if (pid == 0) {
		if (stdin_pipe_writer != -1) {
			close(stdin_pipe_writer);
		}
		if (stdout_pipe_reader != -1) {
			close(stdout_pipe_reader);
		}
		if (stdin_pipe_reader != -1) {
			dup2(stdin_pipe_reader, 0);
			close(stdin_pipe_reader);
		}
		if (stdout_pipe_writer != -1) {
			dup2(stdout_pipe_writer, 1);
			close(stdout_pipe_writer);
		}

		execvp(args[0], args);
		perror(args[0].c_str());
		_exit(-1);
	}
	if (stdin_pipe_reader != -1) {
		close(stdin_pipe_reader);
		stdin_pipe_reader = -1;
	}
	if (stdout_pipe_writer != -1) {
		close(stdout_pipe_writer);
		stdout_pipe_writer = -1;
	}
}

int		Coprocess::wait ()
{
	int		status = 0;
	if (waitpid(pid, &status, 0) == -1) {
		throw System_error("waitpid", "", errno);
	}
	return status;
}

size_t		Coprocess::write_stdin (void* handle, const void* buf, size_t count)
{
	const int	fd = static_cast<Coprocess*>(handle)->stdin_pipe_writer;
	ssize_t		ret;
	while ((ret = write(fd, buf, count)) == -1 && errno == EINTR); // restart if interrupted
	if (ret < 0) {
		throw System_error("write", "", errno);
	}
	return ret;
}

size_t		Coprocess::read_stdout (void* handle, void* buf, size_t count)
{
	const int	fd = static_cast<Coprocess*>(handle)->stdout_pipe_reader;
	ssize_t		ret;
	while ((ret = read(fd, buf, count)) == -1 && errno == EINTR); // restart if interrupted
	if (ret < 0) {
		throw System_error("read", "", errno);
	}
	return ret;
}
