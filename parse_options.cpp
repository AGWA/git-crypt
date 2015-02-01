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

	while (argi < argc && argv[argi][0] == '-') {
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
