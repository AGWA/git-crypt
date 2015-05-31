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

#ifndef GIT_CRYPT_COPROCESS_HPP
#define GIT_CRYPT_COPROCESS_HPP

#include "fhstream.hpp"
#include <windows.h>
#include <vector>

class Coprocess {
	HANDLE		proc_handle;

	HANDLE		stdin_pipe_reader;
	HANDLE		stdin_pipe_writer;
	ofhstream*	stdin_pipe_ostream;
	static size_t	write_stdin (void*, const void*, size_t);

	HANDLE		stdout_pipe_reader;
	HANDLE		stdout_pipe_writer;
	ifhstream*	stdout_pipe_istream;
	static size_t	read_stdout (void*, void*, size_t);

			Coprocess (const Coprocess&);	// Disallow copy
	Coprocess&	operator= (const Coprocess&);	// Disallow assignment
public:
			Coprocess ();
			~Coprocess ();

	std::ostream*	stdin_pipe ();
	void		close_stdin ();

	std::istream*	stdout_pipe ();
	void		close_stdout ();

	void		spawn (const std::vector<std::string>&);

	int		wait ();
};

#endif
