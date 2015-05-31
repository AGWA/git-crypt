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

#include "git-crypt.hpp"
#include "util.hpp"
#include "coprocess.hpp"
#include <string>
#include <iostream>

int exec_command (const std::vector<std::string>& args)
{
	Coprocess	proc;
	proc.spawn(args);
	return proc.wait();
}

int exec_command (const std::vector<std::string>& args, std::ostream& output)
{
	Coprocess	proc;
	std::istream*	proc_stdout = proc.stdout_pipe();
	proc.spawn(args);
	output << proc_stdout->rdbuf();
	return proc.wait();
}

int exec_command_with_input (const std::vector<std::string>& args, const char* p, size_t len)
{
	Coprocess	proc;
	std::ostream*	proc_stdin = proc.stdin_pipe();
	proc.spawn(args);
	proc_stdin->write(p, len);
	proc.close_stdin();
	return proc.wait();
}

std::string	escape_shell_arg (const std::string& str)
{
	std::string	new_str;
	new_str.push_back('"');
	for (std::string::const_iterator it(str.begin()); it != str.end(); ++it) {
		if (*it == '"' || *it == '\\' || *it == '$' || *it == '`') {
			new_str.push_back('\\');
		}
		new_str.push_back(*it);
	}
	new_str.push_back('"');
	return new_str;
}

uint32_t	load_be32 (const unsigned char* p)
{
	return (static_cast<uint32_t>(p[3]) << 0) |
	       (static_cast<uint32_t>(p[2]) << 8) |
	       (static_cast<uint32_t>(p[1]) << 16) |
	       (static_cast<uint32_t>(p[0]) << 24);
}

void		store_be32 (unsigned char* p, uint32_t i)
{
	p[3] = i; i >>= 8;
	p[2] = i; i >>= 8;
	p[1] = i; i >>= 8;
	p[0] = i;
}

bool		read_be32 (std::istream& in, uint32_t& i)
{
	unsigned char buffer[4];
	in.read(reinterpret_cast<char*>(buffer), 4);
	if (in.gcount() != 4) {
		return false;
	}
	i = load_be32(buffer);
	return true;
}

void		write_be32 (std::ostream& out, uint32_t i)
{
	unsigned char buffer[4];
	store_be32(buffer, i);
	out.write(reinterpret_cast<const char*>(buffer), 4);
}

void*		explicit_memset (void* s, int c, std::size_t n)
{
	volatile unsigned char* p = reinterpret_cast<unsigned char*>(s);

	while (n--) {
		*p++ = c;
	}

	return s;
}

static bool	leakless_equals_char (const unsigned char* a, const unsigned char* b, std::size_t len)
{
	volatile int	diff = 0;

	while (len > 0) {
		diff |= *a++ ^ *b++;
		--len;
	}

	return diff == 0;
}

bool 		leakless_equals (const void* a, const void* b, std::size_t len)
{
	return leakless_equals_char(reinterpret_cast<const unsigned char*>(a), reinterpret_cast<const unsigned char*>(b), len);
}

static void	init_std_streams_platform (); // platform-specific initialization

void		init_std_streams ()
{
	// The following two lines are essential for achieving good performance:
	std::ios_base::sync_with_stdio(false);
	std::cin.tie(0);

	std::cin.exceptions(std::ios_base::badbit);
	std::cout.exceptions(std::ios_base::badbit);

	init_std_streams_platform();
}

#ifdef _WIN32
#include "util-win32.cpp"
#else
#include "util-unix.cpp"
#endif
