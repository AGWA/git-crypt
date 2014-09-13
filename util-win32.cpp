/*
 * Copyright 2014 Andrew Ayer
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

#include <io.h>
#include <stdio.h>
#include <fcntl.h>
#include <windows.h>
#include <vector>
#include <cstring>

std::string System_error::message () const
{
	std::string	mesg(action);
	if (!target.empty()) {
		mesg += ": ";
		mesg += target;
	}
	if (error) {
		LPTSTR	error_message;
		FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPTSTR>(&error_message),
			0,
			NULL);
		mesg += error_message;
		LocalFree(error_message);
	}
	return mesg;
}

void	temp_fstream::open (std::ios_base::openmode mode)
{
	close();

	char			tmpdir[MAX_PATH + 1];

	DWORD			ret = GetTempPath(sizeof(tmpdir), tmpdir);
	if (ret == 0) {
		throw System_error("GetTempPath", "", GetLastError());
	} else if (ret > sizeof(tmpdir) - 1) {
		throw System_error("GetTempPath", "", ERROR_BUFFER_OVERFLOW);
	}

	char			tmpfilename[MAX_PATH + 1];
	if (GetTempFileName(tmpdir, TEXT("git-crypt"), 0, tmpfilename) == 0) {
		throw System_error("GetTempFileName", "", GetLastError());
	}

	filename = tmpfilename;

	std::fstream::open(filename.c_str(), mode);
	if (!std::fstream::is_open()) {
		DeleteFile(filename.c_str());
		throw System_error("std::fstream::open", filename, 0);
	}
}

void	temp_fstream::close ()
{
	if (std::fstream::is_open()) {
		std::fstream::close();
		DeleteFile(filename.c_str());
	}
}

void	mkdir_parent (const std::string& path)
{
	std::string::size_type		slash(path.find('/', 1));
	while (slash != std::string::npos) {
		std::string		prefix(path.substr(0, slash));
		if (GetFileAttributes(prefix.c_str()) == INVALID_FILE_ATTRIBUTES) {
			// prefix does not exist, so try to create it
			if (!CreateDirectory(prefix.c_str(), NULL)) {
				throw System_error("CreateDirectory", prefix, GetLastError());
			}
		}

		slash = path.find('/', slash + 1);
	}
}

std::string our_exe_path ()
{
	std::vector<char>	buffer(128);
	size_t			len;

	while ((len = GetModuleFileNameA(NULL, &buffer[0], buffer.size())) == buffer.size()) {
		// buffer may have been truncated - grow and try again
		buffer.resize(buffer.size() * 2);
	}
	if (len == 0) {
		throw System_error("GetModuleFileNameA", "", GetLastError());
	}

	return std::string(buffer.begin(), buffer.begin() + len);
}

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

static int wait_for_child (HANDLE child_handle)
{
	if (WaitForSingleObject(child_handle, INFINITE) == WAIT_FAILED) {
		throw System_error("WaitForSingleObject", "", GetLastError());
	}

	DWORD			exit_code;
	if (!GetExitCodeProcess(child_handle, &exit_code)) {
		throw System_error("GetExitCodeProcess", "", GetLastError());
	}

	return exit_code;
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

	if (!CreateProcessA(NULL,		// application name (NULL to use command line)
				const_cast<char*>(cmdline.c_str()),
				NULL,		// process security attributes
				NULL,		// primary thread security attributes
				TRUE,		// handles are inherited
				0,		// creation flags
				NULL,		// use parent's environment
				NULL,		// use parent's current directory
				&start_info,
				&proc_info)) {
		throw System_error("CreateProcess", cmdline, GetLastError());
	}

	CloseHandle(proc_info.hThread);

	return proc_info.hProcess;
}

int exec_command (const std::vector<std::string>& command)
{
	HANDLE			child_handle = spawn_command(command, NULL, NULL, NULL);
	int			exit_code = wait_for_child(child_handle);
	CloseHandle(child_handle);
	return exit_code;
}

int exec_command (const std::vector<std::string>& command, std::ostream& output)
{
	HANDLE			stdout_pipe_reader = NULL;
	HANDLE			stdout_pipe_writer = NULL;
	SECURITY_ATTRIBUTES	sec_attr;

	// Set the bInheritHandle flag so pipe handles are inherited.
	sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
	sec_attr.bInheritHandle = TRUE;
	sec_attr.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDOUT.
	if (!CreatePipe(&stdout_pipe_reader, &stdout_pipe_writer, &sec_attr, 0)) {
		throw System_error("CreatePipe", "", GetLastError());
	}

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if (!SetHandleInformation(stdout_pipe_reader, HANDLE_FLAG_INHERIT, 0)) {
		throw System_error("SetHandleInformation", "", GetLastError());
	}

	HANDLE			child_handle = spawn_command(command, NULL, stdout_pipe_writer, NULL);
	CloseHandle(stdout_pipe_writer);

	// Read from stdout_pipe_reader.
	// Note that ReadFile on a pipe may return with bytes_read==0 if the other
	// end of the pipe writes zero bytes, so don't break out of the read loop
	// when this happens.  When the other end of the pipe closes, ReadFile
	// fails with ERROR_BROKEN_PIPE.
	char			buffer[1024];
	DWORD			bytes_read;
	while (ReadFile(stdout_pipe_reader, buffer, sizeof(buffer), &bytes_read, NULL)) {
		output.write(buffer, bytes_read);
	}
	const DWORD		read_error = GetLastError();
	if (read_error != ERROR_BROKEN_PIPE) {
		throw System_error("ReadFile", "", read_error);
	}

	CloseHandle(stdout_pipe_reader);

	int			exit_code = wait_for_child(child_handle);
	CloseHandle(child_handle);
	return exit_code;
}

int exec_command_with_input (const std::vector<std::string>& command, const char* p, size_t len)
{
	HANDLE			stdin_pipe_reader = NULL;
	HANDLE			stdin_pipe_writer = NULL;
	SECURITY_ATTRIBUTES	sec_attr;

	// Set the bInheritHandle flag so pipe handles are inherited.
	sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
	sec_attr.bInheritHandle = TRUE;
	sec_attr.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDIN.
	if (!CreatePipe(&stdin_pipe_reader, &stdin_pipe_writer, &sec_attr, 0)) {
		throw System_error("CreatePipe", "", GetLastError());
	}

	// Ensure the write handle to the pipe for STDIN is not inherited.
	if (!SetHandleInformation(stdin_pipe_writer, HANDLE_FLAG_INHERIT, 0)) {
		throw System_error("SetHandleInformation", "", GetLastError());
	}

	HANDLE			child_handle = spawn_command(command, stdin_pipe_reader, NULL, NULL);
	CloseHandle(stdin_pipe_reader);

	// Write to stdin_pipe_writer.
	while (len > 0) {
		DWORD		bytes_written;
		if (!WriteFile(stdin_pipe_writer, p, len, &bytes_written, NULL)) {
			throw System_error("WriteFile", "", GetLastError());
		}
		p += bytes_written;
		len -= bytes_written;
	}

	CloseHandle(stdin_pipe_writer);

	int			exit_code = wait_for_child(child_handle);
	CloseHandle(child_handle);
	return exit_code;
}

bool successful_exit (int status)
{
	return status == 0;
}

void	touch_file (const std::string& filename)
{
	HANDLE	fh = CreateFileA(filename.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (fh == INVALID_HANDLE_VALUE) {
		throw System_error("CreateFileA", filename, GetLastError());
	}
	SYSTEMTIME	system_time;
	GetSystemTime(&system_time);
	FILETIME	file_time;
	SystemTimeToFileTime(&system_time, &file_time);

	if (!SetFileTime(fh, NULL, NULL, &file_time)) {
		DWORD	error = GetLastError();
		CloseHandle(fh);
		throw System_error("SetFileTime", filename, error);
	}
	CloseHandle(fh);
}

void	remove_file (const std::string& filename)
{
	if (!DeleteFileA(filename.c_str())) {
		throw System_error("DeleteFileA", filename, GetLastError());
	}
}

static void	init_std_streams_platform ()
{
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
}

void create_protected_file (const char* path) // TODO
{
}

int util_rename (const char* from, const char* to)
{
	// On Windows OS, it is necessary to ensure target file doesn't exist
	unlink(to);
	return rename(from, to);
}

std::vector<std::string> get_directory_contents (const char* path)
{
	std::vector<std::string>	filenames;
	std::string			patt(path);
	if (!patt.empty() && patt[patt.size() - 1] != '/' && patt[patt.size() - 1] != '\\') {
		patt.push_back('\\');
	}
	patt.push_back('*');

	WIN32_FIND_DATAA		ffd;
	HANDLE				h = FindFirstFileA(patt.c_str(), &ffd);
	if (h == INVALID_HANDLE_VALUE) {
		throw System_error("FindFirstFileA", patt, GetLastError());
	}
	do {
		if (std::strcmp(ffd.cFileName, ".") != 0 && std::strcmp(ffd.cFileName, "..") != 0) {
			filenames.push_back(ffd.cFileName);
		}
	} while (FindNextFileA(h, &ffd) != 0);

	DWORD				err = GetLastError();
	if (err != ERROR_NO_MORE_FILES) {
		throw System_error("FileNextFileA", patt, err);
	}
	FindClose(h);
	return filenames;
}
