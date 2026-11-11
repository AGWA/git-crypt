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
#include <aclapi.h>
#include <bcrypt.h>
#include <vector>
#include <cstring>
#include <cerrno>
#include <algorithm>

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

class win32_handle {
	HANDLE		handle;
	bool		remove_on_close;
	win32_handle (const win32_handle&) = delete;
	win32_handle& operator= (const win32_handle&) = delete;
public:
	explicit win32_handle (HANDLE value, bool remove = false) : handle(value), remove_on_close(remove) { }
	~win32_handle ()
	{
		if (handle != INVALID_HANDLE_VALUE) {
			if (remove_on_close) {
				FILE_DISPOSITION_INFO disposition = {TRUE};
				SetFileInformationByHandle(handle, FileDispositionInfo, &disposition, sizeof(disposition));
			}
			CloseHandle(handle);
		}
	}
	HANDLE get () const { return handle; }
	HANDLE release () { HANDLE value = handle; handle = INVALID_HANDLE_VALUE; return value; }
	void keep () { remove_on_close = false; }
	void close (const char* path)
	{
		if (!CloseHandle(release())) {
			throw System_error("CloseHandle", path, GetLastError());
		}
	}
};

static HANDLE open_protected_file (const char* path, DWORD creation = OPEN_ALWAYS,
				  DWORD sharing = 0, DWORD extra_access = 0)
{
	HANDLE		token;
	if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token)) {
		DWORD		error = GetLastError();
		if (error != ERROR_NO_TOKEN) {
			throw System_error("OpenThreadToken", path, error);
		}
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
			throw System_error("OpenProcessToken", path, GetLastError());
		}
	}
	win32_handle		token_handle(token);
	DWORD			size = 0;
	GetTokenInformation(token, TokenUser, nullptr, 0, &size);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) {
		throw System_error("GetTokenInformation", path, GetLastError());
	}
	std::vector<unsigned char> token_info(size);
	if (!GetTokenInformation(token, TokenUser, token_info.data(), size, &size)) {
		throw System_error("GetTokenInformation", path, GetLastError());
	}
	PSID			user = reinterpret_cast<TOKEN_USER*>(token_info.data())->User.Sid;
	std::vector<unsigned char> acl_bytes(sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD) + GetLengthSid(user));
	PACL			acl = reinterpret_cast<PACL>(acl_bytes.data());
	SECURITY_DESCRIPTOR	descriptor;
	if (!InitializeAcl(acl, acl_bytes.size(), ACL_REVISION) ||
	    !AddAccessAllowedAce(acl, ACL_REVISION, FILE_ALL_ACCESS, user) ||
	    !InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION) ||
	    !SetSecurityDescriptorOwner(&descriptor, user, FALSE) ||
	    !SetSecurityDescriptorDacl(&descriptor, TRUE, acl, FALSE) ||
	    !SetSecurityDescriptorControl(&descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED)) {
		throw System_error("Create security descriptor", path, GetLastError());
	}
	SECURITY_ATTRIBUTES	attributes = {sizeof(SECURITY_ATTRIBUTES), &descriptor, FALSE};
	win32_handle		file(CreateFileA(path, GENERIC_WRITE | READ_CONTROL | WRITE_DAC | extra_access,
				sharing, &attributes, creation, FILE_FLAG_OPEN_REPARSE_POINT, nullptr),
				creation == CREATE_NEW);
	if (file.get() == INVALID_HANDLE_VALUE) {
		throw System_error("CreateFileA", path, GetLastError());
	}
	BY_HANDLE_FILE_INFORMATION info;
	if (!GetFileInformationByHandle(file.get(), &info)) {
		throw System_error("GetFileInformationByHandle", path, GetLastError());
	}
	if (GetFileType(file.get()) != FILE_TYPE_DISK ||
	    (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) ||
	    info.nNumberOfLinks != 1) {
		throw System_error("Unsafe key file type or hard links", path, ERROR_ACCESS_DENIED);
	}
	PSID			owner = nullptr;
	PSECURITY_DESCRIPTOR	security = nullptr;
	DWORD			error = GetSecurityInfo(file.get(), SE_FILE_OBJECT,
				OWNER_SECURITY_INFORMATION, &owner, nullptr, nullptr, nullptr, &security);
	if (error != ERROR_SUCCESS) {
		throw System_error("GetSecurityInfo", path, error);
	}
	bool			owned = owner && EqualSid(owner, user);
	LocalFree(security);
	if (!owned) {
		throw System_error("Key file is not owned by current user", path, ERROR_ACCESS_DENIED);
	}
	error = SetSecurityInfo(file.get(), SE_FILE_OBJECT,
		DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, acl, nullptr);
	if (error != ERROR_SUCCESS) {
		throw System_error("SetSecurityInfo", path, error);
	}
	PACL			actual_acl = nullptr;
	error = GetSecurityInfo(file.get(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
		nullptr, nullptr, &actual_acl, nullptr, &security);
	if (error != ERROR_SUCCESS) {
		throw System_error("GetSecurityInfo", path, error);
	}
	SECURITY_DESCRIPTOR_CONTROL control;
	DWORD			revision;
	void*			ace = nullptr;
	bool			protected_acl = GetSecurityDescriptorControl(security, &control, &revision) &&
		(control & SE_DACL_PROTECTED) && actual_acl && actual_acl->AceCount == 1 &&
		GetAce(actual_acl, 0, &ace);
	if (protected_acl) {
		ACCESS_ALLOWED_ACE* allowed = static_cast<ACCESS_ALLOWED_ACE*>(ace);
		protected_acl = allowed->Header.AceType == ACCESS_ALLOWED_ACE_TYPE &&
			allowed->Header.AceFlags == 0 && allowed->Mask == FILE_ALL_ACCESS &&
			EqualSid(&allowed->SidStart, user);
	}
	LocalFree(security);
	if (!protected_acl) {
		throw System_error("Key file DACL was not protected", path, ERROR_ACCESS_DENIED);
	}
	return file.release();
}

void create_protected_file (const char* path)
{
	win32_handle		file(open_protected_file(path));
	file.close(path);
}

bool write_protected_file (const char* path, const std::string& contents)
{
	DWORD			path_size = GetFullPathNameA(path, 0, nullptr, nullptr);
	if (path_size == 0) {
		throw System_error("GetFullPathNameA", path, GetLastError());
	}
	std::vector<char>	full_path(path_size);
	DWORD			path_length = GetFullPathNameA(path, full_path.size(), full_path.data(), nullptr);
	if (path_length == 0 || path_length >= full_path.size()) {
		throw System_error("GetFullPathNameA", path, ERROR_INVALID_NAME);
	}
	std::string		target(full_path.data());
	std::string::size_type	slash = target.find_last_of("\\/");
	if (slash == std::string::npos) {
		throw System_error("GetFullPathNameA", path, ERROR_INVALID_NAME);
	}
	HANDLE			existing = INVALID_HANDLE_VALUE;
	try {
		existing = open_protected_file(target.c_str(), OPEN_EXISTING, FILE_SHARE_DELETE);
	} catch (const System_error& error) {
		if (error.action != "CreateFileA" || error.error != ERROR_FILE_NOT_FOUND) {
			throw;
		}
	}
	win32_handle		previous(existing);
	HANDLE			temporary = INVALID_HANDLE_VALUE;
	for (unsigned int attempt = 0; attempt < 32; ++attempt) {
		unsigned char random[16];
		if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
			throw System_error("BCryptGenRandom", path, ERROR_GEN_FAILURE);
		}
		static const char hex[] = "0123456789abcdef";
		std::string name = target.substr(0, slash + 1) + ".git-crypt-";
		for (size_t index = 0; index < sizeof(random); ++index) {
			name.push_back(hex[random[index] >> 4]);
			name.push_back(hex[random[index] & 15]);
		}
		name += ".tmp";
		try {
			temporary = open_protected_file(name.c_str(), CREATE_NEW, 0, DELETE);
			break;
		} catch (const System_error& error) {
			if (error.action != "CreateFileA" ||
			    (error.error != ERROR_FILE_EXISTS && error.error != ERROR_ALREADY_EXISTS)) {
				throw;
			}
		}
	}
	if (temporary == INVALID_HANDLE_VALUE) {
		throw System_error("Create temporary key file", path, ERROR_FILE_EXISTS);
	}
	win32_handle		file(temporary, true);
	size_t			offset = 0;
	while (offset < contents.size()) {
		DWORD		length = static_cast<DWORD>(std::min<size_t>(contents.size() - offset, MAXDWORD));
		DWORD		written;
		if (!WriteFile(file.get(), contents.data() + offset, length, &written, nullptr)) {
			throw System_error("WriteFile", path, GetLastError());
		}
		if (written == 0) {
			throw System_error("WriteFile", path, ERROR_WRITE_FAULT);
		}
		offset += written;
	}
	if (!FlushFileBuffers(file.get())) {
		throw System_error("FlushFileBuffers", path, GetLastError());
	}
	int			wide_size = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, target.c_str(), -1, nullptr, 0);
	if (wide_size == 0) {
		throw System_error("MultiByteToWideChar", path, GetLastError());
	}
	std::vector<wchar_t>	wide_path(wide_size);
	if (!MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, target.c_str(), -1, wide_path.data(), wide_size)) {
		throw System_error("MultiByteToWideChar", path, GetLastError());
	}
	DWORD			name_bytes = (wide_size - 1) * sizeof(wchar_t);
	std::vector<unsigned char> rename_bytes(sizeof(FILE_RENAME_INFO) + name_bytes);
	FILE_RENAME_INFO*	rename_info = reinterpret_cast<FILE_RENAME_INFO*>(rename_bytes.data());
	rename_info->ReplaceIfExists = previous.get() != INVALID_HANDLE_VALUE;
	rename_info->RootDirectory = nullptr;
	rename_info->FileNameLength = name_bytes;
	std::memcpy(rename_info->FileName, wide_path.data(), name_bytes);
	if (!SetFileInformationByHandle(file.get(), FileRenameInfo, rename_info, rename_bytes.size())) {
		throw System_error("Replace key file", path, GetLastError());
	}
	file.keep();
	file.close(path);
	return true;
}

int util_rename (const char* from, const char* to)
{
	if (MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		return 0;
	}
	switch (GetLastError()) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		errno = ENOENT;
		break;
	case ERROR_NOT_SAME_DEVICE:
		errno = EXDEV;
		break;
	case ERROR_ACCESS_DENIED:
	case ERROR_SHARING_VIOLATION:
		errno = EACCES;
		break;
	default:
		errno = EIO;
	}
	return -1;
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
