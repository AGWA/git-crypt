#include "../util.hpp"
#include "../key.hpp"
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <cerrno>
#include <set>

enum class io_failure { none, write, flush, rename, competing_target, temporary_collision };

static io_failure injected_failure = io_failure::none;
static bool partial_write = false;
static bool collision_created = false;
static std::string competing_path;
static std::string collision_path;

static BOOL WINAPI test_write_file (HANDLE file, LPCVOID data, DWORD size, LPDWORD written, LPOVERLAPPED overlapped)
{
	if (injected_failure == io_failure::write) {
		if (partial_write) {
			SetLastError(ERROR_DISK_FULL);
			return FALSE;
		}
		partial_write = true;
		return WriteFile(file, data, size > 7 ? 7 : size, written, overlapped);
	}
	return WriteFile(file, data, size, written, overlapped);
}

static BOOL WINAPI test_flush_file (HANDLE file)
{
	if (injected_failure == io_failure::flush) {
		SetLastError(ERROR_WRITE_FAULT);
		return FALSE;
	}
	return FlushFileBuffers(file);
}

static BOOL WINAPI test_set_file_information (HANDLE file, FILE_INFO_BY_HANDLE_CLASS kind, LPVOID data, DWORD size)
{
	if (kind == FileRenameInfo) {
		if (injected_failure == io_failure::rename) {
			SetLastError(ERROR_ACCESS_DENIED);
			return FALSE;
		}
		if (injected_failure == io_failure::competing_target) {
			std::ofstream competitor(competing_path.c_str(), std::ios::binary);
			competitor << "competing key";
			competitor.close();
			if (!competitor) {
				throw std::runtime_error("Unable to create competing target");
			}
		}
	}
	return SetFileInformationByHandle(file, kind, data, size);
}

static HANDLE WINAPI test_create_file (LPCSTR name, DWORD access, DWORD sharing, LPSECURITY_ATTRIBUTES security,
				      DWORD creation, DWORD flags, HANDLE template_file)
{
	if (creation == CREATE_NEW && injected_failure == io_failure::temporary_collision && !collision_created) {
		HANDLE competitor = CreateFileA(name, access, sharing, security, creation, flags, template_file);
		if (competitor == INVALID_HANDLE_VALUE) {
			throw std::runtime_error("Unable to create temporary-name collision");
		}
		DWORD written;
		BOOL stored = WriteFile(competitor, "collision", 9, &written, nullptr);
		CloseHandle(competitor);
		if (!stored || written != 9) {
			throw std::runtime_error("Unable to write colliding file");
		}
		collision_created = true;
		collision_path = name;
		SetLastError(ERROR_FILE_EXISTS);
		return INVALID_HANDLE_VALUE;
	}
	return CreateFileA(name, access, sharing, security, creation, flags, template_file);
}

#define WriteFile test_write_file
#define FlushFileBuffers test_flush_file
#define SetFileInformationByHandle test_set_file_information
#define CreateFileA test_create_file
#include "../util.cpp"
#undef CreateFileA
#undef SetFileInformationByHandle
#undef FlushFileBuffers
#undef WriteFile

static void require (bool condition, const char* message)
{
	if (!condition) {
		throw std::runtime_error(message);
	}
}

class test_handle {
	HANDLE handle;
	test_handle (const test_handle&) = delete;
	test_handle& operator= (const test_handle&) = delete;
public:
	explicit test_handle (HANDLE value) : handle(value) { }
	~test_handle () { if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); } }
	HANDLE get () const { return handle; }
};

class test_directory {
	std::string path;
public:
	test_directory ()
	{
		char temporary[MAX_PATH + 1];
		DWORD length = GetTempPathA(sizeof(temporary), temporary);
		require(length > 0 && length < sizeof(temporary), "GetTempPathA failed");
		char name[MAX_PATH + 1];
		require(GetTempFileNameA(temporary, "gct", 0, name) != 0, "GetTempFileNameA failed");
		require(DeleteFileA(name) && CreateDirectoryA(name, nullptr), "CreateDirectoryA failed");
		path = name;
	}
	~test_directory ()
	{
		WIN32_FIND_DATAA entry;
		HANDLE search = FindFirstFileA((path + "\\*").c_str(), &entry);
		if (search != INVALID_HANDLE_VALUE) {
			do {
				std::string name = entry.cFileName;
				if (name != "." && name != "..") {
					std::string file = path + "\\" + name;
					SetFileAttributesA(file.c_str(), FILE_ATTRIBUTE_NORMAL);
					DeleteFileA(file.c_str());
				}
			} while (FindNextFileA(search, &entry));
			FindClose(search);
		}
		RemoveDirectoryA(path.c_str());
	}
	std::string file (const char* name) const { return path + "\\" + name; }
	const char* name () const { return path.c_str(); }
};

static std::string read_file (const std::string& path)
{
	std::ifstream input(path.c_str(), std::ios::binary);
	require(input.is_open(), "Unable to read test file");
	return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

static void allow_everyone (const char* path)
{
	PSECURITY_DESCRIPTOR descriptor = nullptr;
	require(ConvertStringSecurityDescriptorToSecurityDescriptorA("D:P(A;OICI;FA;;;WD)",
		SDDL_REVISION_1, &descriptor, nullptr) != 0, "Convert security descriptor failed");
	BOOL result = SetFileSecurityA(path, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, descriptor);
	LocalFree(descriptor);
	require(result != 0, "Unable to set permissive test DACL");
}

static void check_dacl (const char* path)
{
	test_handle file(CreateFileA(path, READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, 0, nullptr));
	require(file.get() != INVALID_HANDLE_VALUE, "Unable to inspect DACL");
	PACL acl = nullptr;
	PSID owner = nullptr;
	PSECURITY_DESCRIPTOR descriptor = nullptr;
	require(GetSecurityInfo(file.get(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
		&owner, nullptr, &acl, nullptr, &descriptor) == ERROR_SUCCESS, "GetSecurityInfo failed");
	SECURITY_DESCRIPTOR_CONTROL control;
	DWORD revision;
	void* raw_ace = nullptr;
	bool valid = GetSecurityDescriptorControl(descriptor, &control, &revision) &&
		(control & SE_DACL_PROTECTED) && acl && acl->AceCount == 1 && GetAce(acl, 0, &raw_ace);
	HANDLE token;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
		LocalFree(descriptor);
		throw std::runtime_error("OpenProcessToken failed");
	}
	test_handle token_handle(token);
	DWORD size = 0;
	GetTokenInformation(token, TokenUser, nullptr, 0, &size);
	std::vector<unsigned char> information(size);
	if (!GetTokenInformation(token, TokenUser, information.data(), size, &size)) {
		LocalFree(descriptor);
		throw std::runtime_error("GetTokenInformation failed");
	}
	PSID user = reinterpret_cast<TOKEN_USER*>(information.data())->User.Sid;
	if (valid) {
		ACCESS_ALLOWED_ACE* ace = static_cast<ACCESS_ALLOWED_ACE*>(raw_ace);
		valid = ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE && ace->Header.AceFlags == 0 &&
			ace->Mask == FILE_ALL_ACCESS && EqualSid(&ace->SidStart, user) && owner && EqualSid(owner, user);
	}
	LocalFree(descriptor);
	require(valid, "DACL must grant only the current user full access without inheritance");
}

template<typename Operation>
static void expect_failure (Operation operation)
{
	bool failed = false;
	try {
		operation();
	} catch (const System_error&) {
		failed = true;
	}
	require(failed, "Unsafe operation unexpectedly succeeded");
}

static void test_keys (const test_directory& directory)
{
	std::string path = directory.file("key");
	create_protected_file(path.c_str());
	require(read_file(path).empty(), "New protected file is not empty");
	check_dacl(path.c_str());
	write_protected_file(path.c_str(), "existing contents");
	allow_everyone(path.c_str());
	create_protected_file(path.c_str());
	require(read_file(path) == "existing contents", "Securing an existing file truncated it");
	check_dacl(path.c_str());
	write_protected_file(path.c_str(), "short");
	require(read_file(path) == "short", "Overwrite left stale trailing bytes");

	Key_file key;
	key.generate();
	key.set_key_name("a-long-test-name");
	require(key.store_to_file(path.c_str()), "Key_file::store_to_file failed");
	require(read_file(path) == key.store_to_string(), "Key format changed during file storage");
	key.set_key_name(nullptr);
	allow_everyone(path.c_str());
	require(key.store_to_file(path.c_str()), "Key overwrite failed");
	check_dacl(path.c_str());
	require(read_file(path) == key.store_to_string(), "Key overwrite left trailing bytes");
	Key_file loaded;
	require(loaded.load_from_file(path.c_str()), "Key reload failed");
	require(loaded.store_to_string() == key.store_to_string(), "Key round trip changed contents");
	{
		test_handle reader(CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr, OPEN_EXISTING, 0, nullptr));
		require(reader.get() != INVALID_HANDLE_VALUE, "Unable to hold existing reader");
		expect_failure([&] { key.store_to_file(path.c_str()); });
	}
	require(read_file(path) == key.store_to_string(), "Sharing failure destroyed existing key");
	require(SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_READONLY) != 0, "Set readonly failed");
	expect_failure([&] { key.store_to_file(path.c_str()); });
	require(SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL) != 0, "Clear readonly failed");
	require(read_file(path) == key.store_to_string(), "Readonly failure destroyed existing key");
	expect_failure([&] { key.store_to_file(directory.file("missing\\key").c_str()); });
	expect_failure([&] { key.store_to_file(directory.name()); });

	std::string alias = directory.file("hardlink");
	require(CreateHardLinkA(alias.c_str(), path.c_str(), nullptr) != 0, "CreateHardLinkA failed");
	expect_failure([&] { key.store_to_file(alias.c_str()); });
	require(read_file(path) == key.store_to_string(), "Hardlink rejection destroyed existing key");
	require(DeleteFileA(alias.c_str()) != 0, "Delete hardlink failed");

	std::string symlink = directory.file("symlink");
	if (CreateSymbolicLinkA(symlink.c_str(), path.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
		expect_failure([&] { key.store_to_file(symlink.c_str()); });
		require(read_file(path) == key.store_to_string(), "Symlink rejection modified its target");
	} else {
		require(GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "CreateSymbolicLinkA failed unexpectedly");
		std::cout << "SKIP: symlink test requires Developer Mode or symlink privilege\n";
	}
}

static void test_rename (const test_directory& directory)
{
	std::string target = directory.file("target");
	std::string source = directory.file("source");
	write_protected_file(target.c_str(), "old target");
	std::string missing = directory.file("missing-source");
	errno = 0;
	require(util_rename(missing.c_str(), target.c_str()) == -1 && errno == ENOENT,
		"Missing-source rename did not report failure");
	require(read_file(target) == "old target", "Missing-source rename removed target");
	write_protected_file(source.c_str(), "replacement");
	{
		test_handle locked(CreateFileA(target.c_str(), GENERIC_READ, FILE_SHARE_READ,
			nullptr, OPEN_EXISTING, 0, nullptr));
		require(locked.get() != INVALID_HANDLE_VALUE, "Unable to lock rename target");
		require(util_rename(source.c_str(), target.c_str()) == -1, "Locked-target rename succeeded");
	}
	require(read_file(target) == "old target", "Failed rename removed target");
	require(read_file(source) == "replacement", "Failed rename removed source");
	allow_everyone(target.c_str());
	require(util_rename(source.c_str(), target.c_str()) == 0, "Replacing target failed");
	require(read_file(target) == "replacement", "Replacement contents incorrect");
	check_dacl(target.c_str());
	require(GetFileAttributesA(source.c_str()) == INVALID_FILE_ATTRIBUTES, "Rename left source behind");
	require(util_rename(target.c_str(), source.c_str()) == 0, "Rename to absent target failed");
	require(read_file(source) == "replacement", "Rename to absent target lost data");
}

static std::set<std::string> directory_entries (const test_directory& directory)
{
	std::vector<std::string> entries = get_directory_contents(directory.name());
	return std::set<std::string>(entries.begin(), entries.end());
}

static void test_transactions (const test_directory& directory)
{
	std::string target = directory.file("transaction");
	const std::string original = "previous key must survive";
	const std::string replacement = "complete replacement key contents";
	for (bool existing : {false, true}) {
		if (existing) {
			write_protected_file(target.c_str(), original);
		}
		for (io_failure failure : {io_failure::write, io_failure::flush, io_failure::rename}) {
			std::set<std::string> before = directory_entries(directory);
			partial_write = false;
			injected_failure = failure;
			expect_failure([&] { write_protected_file(target.c_str(), replacement); });
			injected_failure = io_failure::none;
			require(directory_entries(directory) == before, "Failed transaction left a temporary file or target");
			if (existing) {
				require(read_file(target) == original, "Pre-commit failure changed the existing key");
			} else {
				require(GetFileAttributesA(target.c_str()) == INVALID_FILE_ATTRIBUTES, "Failed transaction created a target");
			}
			if (failure == io_failure::write) {
				require(partial_write, "Partial-write fault was not reached");
			}
		}
	}

	test_handle previous(CreateFileA(target.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, 0, nullptr));
	require(previous.get() != INVALID_HANDLE_VALUE, "Unable to inspect original key identity");
	BY_HANDLE_FILE_INFORMATION before;
	require(GetFileInformationByHandle(previous.get(), &before) != 0, "Unable to read original key identity");
	require(write_protected_file(target.c_str(), replacement), "Transaction commit failed");
	test_handle current(CreateFileA(target.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, 0, nullptr));
	require(current.get() != INVALID_HANDLE_VALUE, "Unable to inspect committed key identity");
	BY_HANDLE_FILE_INFORMATION after;
	require(GetFileInformationByHandle(current.get(), &after) != 0, "Unable to read committed key identity");
	require(before.nFileIndexHigh != after.nFileIndexHigh || before.nFileIndexLow != after.nFileIndexLow,
		"Transaction modified the original file instead of replacing it");
	require(read_file(target) == replacement, "Committed key contents are incomplete");
	check_dacl(target.c_str());

	competing_path = directory.file("competing-target");
	std::set<std::string> entries = directory_entries(directory);
	injected_failure = io_failure::competing_target;
	expect_failure([&] { write_protected_file(competing_path.c_str(), replacement); });
	injected_failure = io_failure::none;
	require(read_file(competing_path) == "competing key", "Transaction overwrote a newly appearing target");
	check_dacl(target.c_str());
	require(DeleteFileA(competing_path.c_str()) != 0, "Unable to delete competing target");
	require(directory_entries(directory) == entries, "Competing-target failure leaked a temporary file");

	injected_failure = io_failure::temporary_collision;
	require(write_protected_file(directory.file("collision-target").c_str(), replacement), "Temporary-name retry failed");
	injected_failure = io_failure::none;
	require(collision_created && read_file(collision_path) == "collision", "Transaction modified a colliding file");
	require(DeleteFileA(collision_path.c_str()) != 0, "Unable to delete colliding file");
}

int main ()
{
	try {
		test_directory directory;
		allow_everyone(directory.name());
		test_keys(directory);
		test_rename(directory);
		test_transactions(directory);
		std::cout << "Windows key-management regression tests passed\n";
		return 0;
	} catch (const System_error& error) {
		std::cerr << error.message() << '\n';
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
	}
	return 1;
}