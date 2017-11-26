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

#include "coprocess-win32.hpp"
#include "util.hpp"


static void escape_cmdline_argument (std::string& cmdline, const std::string& arg)
{
	// For an explanation of Win32's arcane argument quoting rules, see:
	//  http://msdn.microsoft.com/en-us/library/17w5ykft%28v=vs.85%29.aspx
	//  http://msdn.microsoft.com/en-us/library/bb776391%28v=vs.85%29.aspx
	//  http://blogs.msdn.com/b/twistylittlepassagesallalike/archive/2011/04/23/everyone-quotes-arguments-the-wrong-way.aspx
	//  http://blogs.msdn.com/b/oldnewthing/archive/2010/09/17/10063629.aspx
	cmdline.push_back('"');

	std::string::const_iterator	p(arg.begin());
	while (p != arg.end()) {
		if (*p == '"') {
			cmdline.push_back('\\');
			cmdline.push_back('"');
			++p;
		} else if (*p == '\\') {
			unsigned int	num_backslashes = 0;
			while (p != arg.end() && *p == '\\') {
				++num_backslashes;
				++p;
			}
			if (p == arg.end() || *p == '"') {
				// Backslashes need to be escaped
				num_backslashes *= 2;
			}
			while (num_backslashes--) {
				cmdline.push_back('\\');
			}
		} else {
			cmdline.push_back(*p++);
		}
	}

	cmdline.push_back('"');
}

static std::string format_cmdline (const std::vector<std::string>& command)
{
	std::string		cmdline;
	for (std::vector<std::string>::const_iterator arg(command.begin()); arg != command.end(); ++arg) {
		if (arg != command.begin()) {
			cmdline.push_back(' ');
		}
		escape_cmdline_argument(cmdline, *arg);
	}
	return cmdline;
}

static HANDLE spawn_command (const std::vector<std::string>& command, HANDLE stdin_handle, HANDLE stdout_handle, HANDLE stderr_handle)
{
	PROCESS_INFORMATION	proc_info;
	ZeroMemory(&proc_info, sizeof(proc_info));

	STARTUPINFO		start_info;
	ZeroMemory(&start_info, sizeof(start_info));

	start_info.cb = sizeof(STARTUPINFO);
	start_info.hStdInput = stdin_handle ? stdin_handle : GetStdHandle(STD_INPUT_HANDLE);
	start_info.hStdOutput = stdout_handle ? stdout_handle : GetStdHandle(STD_OUTPUT_HANDLE);
	start_info.hStdError = stderr_handle ? stderr_handle : GetStdHandle(STD_ERROR_HANDLE);
	start_info.dwFlags |= STARTF_USESTDHANDLES;

	std::string		cmdline(format_cmdline(command));

	if (!CreateProcessA(nullptr,		// application name (nullptr to use command line)
				const_cast<char*>(cmdline.c_str()),
				nullptr,	// process security attributes
				nullptr,	// primary thread security attributes
				TRUE,		// handles are inherited
				0,		// creation flags
				nullptr,	// use parent's environment
				nullptr,	// use parent's current directory
				&start_info,
				&proc_info)) {
		throw System_error("CreateProcess", cmdline, GetLastError());
	}

	CloseHandle(proc_info.hThread);

	return proc_info.hProcess;
}


Coprocess::Coprocess ()
{
	proc_handle = nullptr;
	stdin_pipe_reader = nullptr;
	stdin_pipe_writer = nullptr;
	stdin_pipe_ostream = nullptr;
	stdout_pipe_reader = nullptr;
	stdout_pipe_writer = nullptr;
	stdout_pipe_istream = nullptr;
}

Coprocess::~Coprocess ()
{
	close_stdin();
	close_stdout();
	if (proc_handle) {
		CloseHandle(proc_handle);
	}
}

std::ostream*	Coprocess::stdin_pipe ()
{
	if (!stdin_pipe_ostream) {
		SECURITY_ATTRIBUTES	sec_attr;

		// Set the bInheritHandle flag so pipe handles are inherited.
		sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
		sec_attr.bInheritHandle = TRUE;
		sec_attr.lpSecurityDescriptor = nullptr;

		// Create a pipe for the child process's STDIN.
		if (!CreatePipe(&stdin_pipe_reader, &stdin_pipe_writer, &sec_attr, 0)) {
			throw System_error("CreatePipe", "", GetLastError());
		}

		// Ensure the write handle to the pipe for STDIN is not inherited.
		if (!SetHandleInformation(stdin_pipe_writer, HANDLE_FLAG_INHERIT, 0)) {
			throw System_error("SetHandleInformation", "", GetLastError());
		}

		stdin_pipe_ostream = new ofhstream(this, write_stdin);
	}
	return stdin_pipe_ostream;
}

void		Coprocess::close_stdin ()
{
	delete stdin_pipe_ostream;
	stdin_pipe_ostream = nullptr;
	if (stdin_pipe_writer) {
		CloseHandle(stdin_pipe_writer);
		stdin_pipe_writer = nullptr;
	}
	if (stdin_pipe_reader) {
		CloseHandle(stdin_pipe_reader);
		stdin_pipe_reader = nullptr;
	}
}

std::istream*	Coprocess::stdout_pipe ()
{
	if (!stdout_pipe_istream) {
		SECURITY_ATTRIBUTES	sec_attr;

		// Set the bInheritHandle flag so pipe handles are inherited.
		sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
		sec_attr.bInheritHandle = TRUE;
		sec_attr.lpSecurityDescriptor = nullptr;

		// Create a pipe for the child process's STDOUT.
		if (!CreatePipe(&stdout_pipe_reader, &stdout_pipe_writer, &sec_attr, 0)) {
			throw System_error("CreatePipe", "", GetLastError());
		}

		// Ensure the read handle to the pipe for STDOUT is not inherited.
		if (!SetHandleInformation(stdout_pipe_reader, HANDLE_FLAG_INHERIT, 0)) {
			throw System_error("SetHandleInformation", "", GetLastError());
		}

		stdout_pipe_istream = new ifhstream(this, read_stdout);
	}
	return stdout_pipe_istream;
}

void		Coprocess::close_stdout ()
{
	delete stdout_pipe_istream;
	stdout_pipe_istream = nullptr;
	if (stdout_pipe_writer) {
		CloseHandle(stdout_pipe_writer);
		stdout_pipe_writer = nullptr;
	}
	if (stdout_pipe_reader) {
		CloseHandle(stdout_pipe_reader);
		stdout_pipe_reader = nullptr;
	}
}

void		Coprocess::spawn (const std::vector<std::string>& args)
{
	proc_handle = spawn_command(args, stdin_pipe_reader, stdout_pipe_writer, nullptr);
	if (stdin_pipe_reader) {
		CloseHandle(stdin_pipe_reader);
		stdin_pipe_reader = nullptr;
	}
	if (stdout_pipe_writer) {
		CloseHandle(stdout_pipe_writer);
		stdout_pipe_writer = nullptr;
	}
}

int		Coprocess::wait ()
{
	if (WaitForSingleObject(proc_handle, INFINITE) == WAIT_FAILED) {
		throw System_error("WaitForSingleObject", "", GetLastError());
	}

	DWORD			exit_code;
	if (!GetExitCodeProcess(proc_handle, &exit_code)) {
		throw System_error("GetExitCodeProcess", "", GetLastError());
	}

	return exit_code;
}

size_t		Coprocess::write_stdin (void* handle, const void* buf, size_t count)
{
	DWORD		bytes_written;
	if (!WriteFile(static_cast<Coprocess*>(handle)->stdin_pipe_writer, buf, count, &bytes_written, nullptr)) {
		throw System_error("WriteFile", "", GetLastError());
	}
	return bytes_written;
}

size_t		Coprocess::read_stdout (void* handle, void* buf, size_t count)
{
	// Note that ReadFile on a pipe may return with bytes_read==0 if the other
	// end of the pipe writes zero bytes, so retry when this happens.
	// When the other end of the pipe actually closes, ReadFile
	// fails with ERROR_BROKEN_PIPE.
	DWORD bytes_read;
	do {
		if (!ReadFile(static_cast<Coprocess*>(handle)->stdout_pipe_reader, buf, count, &bytes_read, nullptr)) {
			const DWORD	read_error = GetLastError();
			if (read_error != ERROR_BROKEN_PIPE) {
				throw System_error("ReadFile", "", read_error);
			}
			return 0;
		}
	} while (bytes_read == 0);
	return bytes_read;
}
