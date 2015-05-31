/*
 * Copyright 2014 Andrew Ayer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
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

int parse_options (const Options_list& options, int argc, const char** argv);

struct Option_error {
	std::string	option_name;
	std::string	message;

	Option_error (const std::string& n, const std::string& m) : option_name(n), message(m) { }
};

#endif
