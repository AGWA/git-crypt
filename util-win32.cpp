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
			nullptr,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPTSTR>(&error_message),
			0,
			nullptr);
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
			if (!CreateDirectory(prefix.c_str(), nullptr)) {
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

	while ((len = GetModuleFileNameA(nullptr, &buffer[0], buffer.size())) == buffer.size()) {
		// buffer may have been truncated - grow and try again
		buffer.resize(buffer.size() * 2);
	}
	if (len == 0) {
		throw System_error("GetModuleFileNameA", "", GetLastError());
	}

	return std::string(buffer.begin(), buffer.begin() + len);
}

int exit_status (int status)
{
	return status;
}

void	touch_file (const std::string& filename)
{
	HANDLE	fh = CreateFileA(filename.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if (fh == INVALID_HANDLE_VALUE) {
		DWORD	error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND) {
			return;
		} else {
			throw System_error("CreateFileA", filename, error);
		}
	}
	SYSTEMTIME	system_time;
	GetSystemTime(&system_time);
	FILETIME	file_time;
	SystemTimeToFileTime(&system_time, &file_time);

	if (!SetFileTime(fh, nullptr, nullptr, &file_time)) {
		DWORD	error = GetLastError();
		CloseHandle(fh);
		throw System_error("SetFileTime", filename, error);
	}
	CloseHandle(fh);
}

void	remove_file (const std::string& filename)
{
	if (!DeleteFileA(filename.c_str())) {
		DWORD	error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND) {
			return;
		} else {
			throw System_error("DeleteFileA", filename, error);
		}
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
