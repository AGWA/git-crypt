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

#include "commands.hpp"
#include "util.hpp"
#include <cstring>
#include <iostream>
#include <openssl/err.h>

static void print_usage (const char* argv0)
{
	std::clog << "Usage: " << argv0 << " COMMAND [ARGS ...]\n";
	std::clog << "\n";
	std::clog << "Valid commands:\n";
	std::clog << " init [--name=<key-name>] KEYFILE - prepare the current git repo to use git-crypt with this key\n";
	std::clog << " keygen KEYFILE  - generate a git-crypt key in the given file\n";
	std::clog << "\n";
	std::clog << "Plumbing commands (not to be used directly):\n";
	std::clog << " clean KEYFILE\n";
	std::clog << " smudge KEYFILE\n";
	std::clog << " diff KEYFILE FILE\n";
}


int main (int argc, const char** argv)
try {
	// The following two lines are essential for achieving good performance:
	std::ios_base::sync_with_stdio(false);
	std::cin.tie(0);

	std::cin.exceptions(std::ios_base::badbit);
	std::cout.exceptions(std::ios_base::badbit);

	if (argc < 3) {
		print_usage(argv[0]);
		return 2;
	}

	ERR_load_crypto_strings();

	if (strcmp(argv[1], "init") == 0 && argc == 3) {
		init(argv[0], argv[2], "git-crypt");
	} else if (strcmp(argv[1], "init") == 0 && argc == 4 && strncmp(argv[2], "--name=", 7) == 0) {
		init(argv[0], argv[3], argv[2] + 7);
	} else if (strcmp(argv[1], "keygen") == 0 && argc == 3) {
		keygen(argv[2]);
	} else if (strcmp(argv[1], "clean") == 0 && argc == 3) {
		clean(argv[2]);
	} else if (strcmp(argv[1], "smudge") == 0 && argc == 3) {
		smudge(argv[2]);
	} else if (strcmp(argv[1], "diff") == 0 && argc == 4) {
		diff(argv[2], argv[3]);
	} else {
		print_usage(argv[0]);
		return 2;
	}

	return 0;
} catch (const std::ios_base::failure& e) {
	std::cerr << "git-crypt: I/O error: " << e.what() << std::endl;
	return 1;
}


