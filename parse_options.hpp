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

#ifndef PARSE_OPTIONS_HPP
#define PARSE_OPTIONS_HPP

#include <string>
#include <vector>

struct Option_def {
	std::string	name;
	bool*		is_set;
	const char**	value;

	Option_def () : is_set(0), value(0) { }
	Option_def (const std::string& arg_name, bool* arg_is_set)
	: name(arg_name), is_set(arg_is_set), value(0) { }
	Option_def (const std::string& arg_name, const char** arg_value)
	: name(arg_name), is_set(0), value(arg_value) { }
};

typedef std::vector<Option_def> Options_list;

int parse_options (const Options_list& options, int argc, char** argv);

struct Option_error {
	std::string	option_name;
	std::string	message;

	Option_error (const std::string& n, const std::string& m) : option_name(n), message(m) { }
};

#endif
