/*
 * Copyright 2012, 2014 Andrew Ayer
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

#ifndef GIT_CRYPT_UTIL_HPP
#define GIT_CRYPT_UTIL_HPP

#include <string>
#include <ios>
#include <iosfwd>
#include <stdint.h>
#include <sys/types.h>
#include <fstream>
#include <vector>

struct System_error {
	std::string	action;
	std::string	target;
	int		error;

	System_error (const std::string& a, const std::string& t, int e) : action(a), target(t), error(e) { }

	std::string message () const;
};

class temp_fstream : public std::fstream {
	std::string	filename;
public:
	~temp_fstream () { close(); }

	void		open (std::ios_base::openmode);
	void		close ();
};

void		mkdir_parent (const std::string& path); // Create parent directories of path, __but not path itself__
std::string	our_exe_path ();
int		exec_command (const std::vector<std::string>&);
int		exec_command (const std::vector<std::string>&, std::ostream& output);
int		exec_command_with_input (const std::vector<std::string>&, const char* p, size_t len);
int		exit_status (int wait_status); // returns -1 if process did not exit (but was signaled, etc.)
inline bool	successful_exit (int wait_status) { return exit_status(wait_status) == 0; }
void		touch_file (const std::string&); // ignores non-existent files
void		remove_file (const std::string&); // ignores non-existent files
std::string	escape_shell_arg (const std::string&);
uint32_t	load_be32 (const unsigned char*);
void		store_be32 (unsigned char*, uint32_t);
bool		read_be32 (std::istream& in, uint32_t&);
void		write_be32 (std::ostream& out, uint32_t);
void*		explicit_memset (void* s, int c, size_t n);	// memset that won't be optimized away
bool		leakless_equals (const void* a, const void* b, size_t len); // compare bytes w/o leaking timing
void		init_std_streams ();
void		create_protected_file (const char* path); // create empty file accessible only by current user
int		util_rename (const char*, const char*);
std::vector<std::string> get_directory_contents (const char* path);

#endif

