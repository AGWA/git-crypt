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

#include "parse_options.hpp"
#include <cstring>


static const Option_def* find_option (const Options_list& options, const std::string& name)
{
	for (Options_list::const_iterator opt(options.begin()); opt != options.end(); ++opt) {
		if (opt->name == name) {
			return &*opt;
		}
	}
	return 0;
}

int parse_options (const Options_list& options, int argc, const char** argv)
{
	int	argi = 0;

	while (argi < argc && argv[argi][0] == '-' && argv[argi][1] != '\0') {
		if (std::strcmp(argv[argi], "--") == 0) {
			++argi;
			break;
		} else if (std::strncmp(argv[argi], "--", 2) == 0) {
			std::string			option_name;
			const char*			option_value = 0;
			if (const char* eq = std::strchr(argv[argi], '=')) {
				option_name.assign(argv[argi], eq);
				option_value = eq + 1;
			} else {
				option_name = argv[argi];
			}
			++argi;

			const Option_def*		opt(find_option(options, option_name));
			if (!opt) {
				throw Option_error(option_name, "Invalid option");
			}

			if (opt->is_set) {
				*opt->is_set = true;
			}
			if (opt->value) {
				if (option_value) {
					*opt->value = option_value;
				} else {
					if (argi >= argc) {
						throw Option_error(option_name, "Option requires a value");
					}
					*opt->value = argv[argi];
					++argi;
				}
			} else {
				if (option_value) {
					throw Option_error(option_name, "Option takes no value");
				}
			}
		} else {
			const char*			arg = argv[argi] + 1;
			++argi;
			while (*arg) {
				std::string		option_name("-");
				option_name.push_back(*arg);
				++arg;

				const Option_def*	opt(find_option(options, option_name));
				if (!opt) {
					throw Option_error(option_name, "Invalid option");
				}
				if (opt->is_set) {
					*opt->is_set = true;
				}
				if (opt->value) {
					if (*arg) {
						*opt->value = arg;
					} else {
						if (argi >= argc) {
							throw Option_error(option_name, "Option requires a value");
						}
						*opt->value = argv[argi];
						++argi;
					}
					break;
				}
			}
		}
	}
	return argi;
}
