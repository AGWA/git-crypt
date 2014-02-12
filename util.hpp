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

#ifndef _UTIL_H
#define _UTIL_H

#include <string>
#include <ios>
#include <iosfwd>
#include <fstream>


int			exec_command (const char* command, std::ostream& output);
std::string	resolve_path (const char* path);
void		open_tempfile (std::fstream&, std::ios_base::openmode);
std::string	escape_shell_arg (const std::string&);


#ifdef __WIN32__
int			win32_system (const char* command);
void		set_cin_cout_binary_mode (void);

class	temp_fstream : public std::fstream {
public:
				temp_fstream();
	void		open (const char *fname, std::ios_base::openmode mode);
	virtual		~temp_fstream();
private:
	char	*fileName;
};
#endif


#endif

