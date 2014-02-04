#ifdef __WIN32__
#undef __STRICT_ANSI__ //needed for _fullpath
#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>


#define unlink _unlink


void LaunchChildProcess(HANDLE hChildStdOut,
				HANDLE hChildStdIn,
				HANDLE hChildStdErr,
				const std::string command);
void ReadAndHandleOutput(HANDLE hPipeRead, std::ostream& output);
void ArgvQuote (const std::string& Argument,
				std::string& CommandLine,
				bool Force);
int mkstemp(char *name);
void DisplayError(LPCSTR wszPrefix);

//do nothing on windows
static inline mode_t umask(mode_t mask){ return -1; }


int exec_command (const char* command, std::ostream& output)
{
	HANDLE hOutputReadTmp,hOutputRead,hOutputWrite;
	HANDLE hErrorWrite;
	SECURITY_ATTRIBUTES sa;

	// Set up the security attributes struct.
	sa.nLength= sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	// Create the child output pipe.
	if (!CreatePipe(&hOutputReadTmp,&hOutputWrite,&sa,0))
		DisplayError("CreatePipe");


	// Create a duplicate of the output write handle for the std error
	// write handle. This is necessary in case the child application
	// closes one of its std output handles.
	if (!DuplicateHandle(GetCurrentProcess(),hOutputWrite,
				GetCurrentProcess(),&hErrorWrite,0,
				TRUE,DUPLICATE_SAME_ACCESS))
		DisplayError("DuplicateHandle");

	// Create new output read handle. Set
	// the Properties to FALSE. Otherwise, the child inherits the
	// properties and, as a result, non-closeable handles to the pipes
	// are created.
	if (!DuplicateHandle(GetCurrentProcess(),hOutputReadTmp,
				GetCurrentProcess(),
				&hOutputRead, // Address of new handle.
				0,FALSE, // Make it uninheritable.
				DUPLICATE_SAME_ACCESS))
		DisplayError("DuplicateHandle");

	// Close inheritable copies of the handles you do not want to be
	// inherited.
	if (!CloseHandle(hOutputReadTmp)) DisplayError("CloseHandle");

	// Prepare command arg
	std::string cmd("sh.exe -c ");
	ArgvQuote(command, cmd, TRUE);

	// Launch child process with "cmd"
	LaunchChildProcess(hOutputWrite ,NULL ,hErrorWrite, cmd);

	// Close pipe handles (do not continue to modify the parent).
	// You need to make sure that no handles to the write end of the
	// output pipe are maintained in this process or else the pipe will
	// not close when the child process exits and the ReadFile will hang.
	if (!CloseHandle(hOutputWrite)) DisplayError("CloseHandle");
	if (!CloseHandle(hErrorWrite)) DisplayError("CloseHandle");

	// Read the child's output.
	ReadAndHandleOutput(hOutputRead, output);

	if (!CloseHandle(hOutputRead)) DisplayError("CloseHandle");

	return 1;
}

std::string escape_shell_arg(const std::string& str)
{
	std::string escape_str;
	ArgvQuote(str, escape_str, TRUE);
	return escape_str;
}

std::string resolve_path (const char* path)
{
	char retname[_MAX_PATH];
	_fullpath(retname, path, _MAX_PATH);
	return std::string(retname);;	
}

void open_tempfile (std::fstream& file, std::ios_base::openmode mode)
{
	const char*	tmpdir = getenv("TEMP");
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
	
	// On windows we cannot remove open files. Then git-crypt.XXXXXX temporary files will
	// remain on %TEMP% folder for manually remove. It´s not that bad cause 26 different name 
	// limit on windows mkstemp. 
	// unlink(path);
	close(fd);
	delete[] path;
}

/*
 * LaunchChildProcess
 * Sets up STARTUPINFO structure, and launches redirected child.
 */
void LaunchChildProcess(HANDLE hChildStdOut,
				HANDLE hChildStdIn,
				HANDLE hChildStdErr,
				const std::string command)
{
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	BOOL br = FALSE;
	// Set up the start up info struct.
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = hChildStdOut;
	si.hStdInput  = hChildStdIn;
	si.hStdError  = hChildStdErr;

	br = CreateProcess(NULL,  /* module: null means use command line */
						(LPSTR)command.c_str(),  /* modified command line */
						NULL, /* thread handle inheritance */
						NULL, /* thread handle inheritance */
						TRUE, /* handles inheritable? */
						CREATE_UNICODE_ENVIRONMENT,
						NULL, /* environment: use parent */
						NULL, /* starting directory: use parent */
						&si,&pi);

	if (br) {
		WaitForSingleObject(pi.hProcess, INFINITE);
	}
	else
	{
		DisplayError("Error launching process");
	}
	// Close any unnecessary handles.
	if (!CloseHandle(pi.hProcess)) DisplayError("CloseHandle");
	if (!CloseHandle(pi.hThread)) DisplayError("CloseHandle");
}


/*
 * ReadAndHandleOutput
 * Monitors handle for input. Exits when child exits or pipe breaks.
 */
void ReadAndHandleOutput(HANDLE hPipeRead, std::ostream& output)
{
	UCHAR lpBuffer[256];
	DWORD nBytesRead;

	while(TRUE)
	{
		memset(lpBuffer, 0, sizeof(lpBuffer));
		if (!ReadFile(hPipeRead,lpBuffer,sizeof(lpBuffer),&nBytesRead,NULL) || !nBytesRead)
		{
			if (GetLastError() == ERROR_BROKEN_PIPE)
				break; // pipe done - normal exit path.
			else
				DisplayError("ReadFile"); // Something bad happened.
		}

		output.write((const char *)lpBuffer, nBytesRead);	
	}
}

/*
 * ArgvQuote
 */
void ArgvQuote (const std::string& Argument,
				std::string& CommandLine,
				bool Force)
/***
 *
 * Routine Description:
 *
 * This routine appends the given argument to a command line such
 * that CommandLineToArgvW will return the argument string unchanged.
 * Arguments in a command line should be separated by spaces; this
 * function does not add these spaces.
 *
 * Arguments:
 *
 * Argument - Supplies the argument to encode.
 *
 * CommandLine - Supplies the command line to which we append the encoded argument string.
 *
 * Force - Supplies an indication of whether we should quote
 * the argument even if it does not contain any characters that would
 * ordinarily require quoting.
 *
 * Return Value:
 *
 * None.
 *
 * Environment:
 *
 * Arbitrary.
 *
 */
 {
	//
	// Unless we're told otherwise, don't quote unless we actually
	// need to do so --- hopefully avoid problems if programs won't
	// parse quotes properly
	//
	if (Force == false &&
		Argument.empty () == false &&
		Argument.find_first_of (" \t\n\v\"") == Argument.npos)
	{
		CommandLine.append (Argument);
	}
	else {
		CommandLine.push_back ('"');
		std::string::const_iterator It;
		for (It = Argument.begin () ; ; ++It) {
			unsigned NumberBackslashes = 0;
			while (It != Argument.end () && *It == '\\') {
				++It;
				++NumberBackslashes;
			}

			if (It == Argument.end ()) {
				//
				// Escape all backslashes, but let the terminating
				// double quotation mark we add below be interpreted
				// as a metacharacter.
				//
				CommandLine.append (NumberBackslashes * 2, '\\');
				 break;
			}
			else if (*It == '"') {
				//
				// Escape all backslashes and the following
				// double quotation mark.
				//
				CommandLine.append (NumberBackslashes * 2 + 1, '\\');
				CommandLine.push_back (*It);
			}
			else {
				//
				// Backslashes aren't special here.
				//
				CommandLine.append (NumberBackslashes, '\\');
				CommandLine.push_back (*It);
			}
		}
		CommandLine.push_back ('"');
	}
}

int mkstemp(char * filetemplate)
{
	// on windows _mktemp generate only 26 unique filename
	char *filename = _mktemp(filetemplate);
	if (filename == NULL) { 
		return -1;
	}
	return open(filename, _O_RDWR |_O_BINARY | O_CREAT);
}

/*
 * DisplayError
 * Displays the error number and corresponding message.
 */
void DisplayError(LPCSTR szPrefix)
{
	LPSTR lpsz = NULL;
	DWORD cch = 0;
	DWORD dwError = GetLastError();

	cch = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
					| FORMAT_MESSAGE_FROM_SYSTEM
					| FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL, dwError, LANG_NEUTRAL,
					(LPTSTR)&lpsz, 0, NULL);
	if (cch < 1) {
		cch = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
						| FORMAT_MESSAGE_FROM_STRING
						| FORMAT_MESSAGE_ARGUMENT_ARRAY,
						"Code 0x%1!08x!",
						0, LANG_NEUTRAL, (LPTSTR)&lpsz, 0,
						(va_list*)&dwError);
	}
	fprintf(stderr, "%s: %s", szPrefix, lpsz);
	LocalFree((HLOCAL)lpsz);
	ExitProcess(-1);
}

#endif