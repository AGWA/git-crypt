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
#include "commands.hpp"
#include "util.hpp"
#include "crypto.hpp"
#include "key.hpp"
#include "gpg.hpp"
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <openssl/err.h>

const char*	argv0;

static void print_usage (std::ostream& out)
{
	out << "Usage: " << argv0 << " COMMAND [ARGS ...]" << std::endl;
	out << "" << std::endl;
	out << "Standard commands:" << std::endl;
	out << " init             - generate a key, prepare the current repo to use git-crypt" << std::endl;
	out << " unlock KEYFILE   - decrypt the current repo using the given symmetric key" << std::endl;
	out << " export-key FILE  - export the repo's symmetric key to the given file" << std::endl;
	//out << " refresh          - ensure all files in the repo are properly decrypted" << std::endl;
	out << " help             - display this help message" << std::endl;
	out << " help COMMAND     - display help for the given git-crypt command" << std::endl;
	out << "" << std::endl;
	/*
	out << "GPG commands:" << std::endl;
	out << " unlock           - decrypt the current repo using the in-repo GPG-encrypted key" << std::endl;
	out << " add-collab GPGID - add the user with the given GPG key ID as a collaborator" << std::endl;
	out << " rm-collab GPGID  - revoke collaborator status from the given GPG key ID" << std::endl;
	out << " ls-collabs       - list the GPG key IDs of collaborators" << std::endl;
	out << "" << std::endl;
	*/
	out << "Legacy commands:" << std::endl;
	out << " init KEYFILE     - alias for 'unlock KEYFILE'" << std::endl;
	out << " keygen KEYFILE   - generate a git-crypt key in the given file" << std::endl;
	out << " migrate-key FILE - migrate the given legacy key file to the latest format" << std::endl;
	out << "" << std::endl;
	out << "Plumbing commands (not to be used directly):" << std::endl;
	out << " clean [LEGACY-KEYFILE]" << std::endl;
	out << " smudge [LEGACY-KEYFILE]" << std::endl;
	out << " diff [LEGACY-KEYFILE] FILE" << std::endl;
}


int main (int argc, char** argv)
try {
	argv0 = argv[0];

	/*
	 * General initialization
	 */

	// The following two lines are essential for achieving good performance:
	std::ios_base::sync_with_stdio(false);
	std::cin.tie(0);

	std::cin.exceptions(std::ios_base::badbit);
	std::cout.exceptions(std::ios_base::badbit);

	ERR_load_crypto_strings();

	/*
	 * Parse command line arguments
	 */
	const char*		profile = 0;
	int			arg_index = 1;
	while (arg_index < argc && argv[arg_index][0] == '-') {
		if (std::strcmp(argv[arg_index], "--help") == 0) {
			print_usage(std::clog);
			return 0;
		} else if (std::strncmp(argv[arg_index], "--profile=", 10) == 0) {
			profile = argv[arg_index] + 10;
			++arg_index;
		} else if (std::strcmp(argv[arg_index], "-p") == 0 && arg_index + 1 < argc) {
			profile = argv[arg_index + 1];
			arg_index += 2;
		} else if (std::strcmp(argv[arg_index], "--") == 0) {
			++arg_index;
			break;
		} else {
			std::clog << argv0 << ": " << argv[arg_index] << ": Unknown option" << std::endl;
			print_usage(std::clog);
			return 2;
		}
	}

	(void)(profile); // TODO: profile support

	argc -= arg_index;
	argv += arg_index;

	if (argc == 0) {
		print_usage(std::clog);
		return 2;
	}

	/*
	 * Pass off to command handler
	 */
	const char*		command = argv[0];
	--argc;
	++argv;

	// Public commands:
	if (std::strcmp(command, "help") == 0) {
		print_usage(std::clog);
		return 0;
	}
	if (std::strcmp(command, "init") == 0) {
		return init(argc, argv);
	}
	if (std::strcmp(command, "unlock") == 0) {
		return unlock(argc, argv);
	}
	if (std::strcmp(command, "add-collab") == 0) {
		return add_collab(argc, argv);
	}
	if (std::strcmp(command, "rm-collab") == 0) {
		return rm_collab(argc, argv);
	}
	if (std::strcmp(command, "ls-collabs") == 0) {
		return ls_collabs(argc, argv);
	}
	if (std::strcmp(command, "export-key") == 0) {
		return export_key(argc, argv);
	}
	if (std::strcmp(command, "keygen") == 0) {
		return keygen(argc, argv);
	}
	if (std::strcmp(command, "migrate-key") == 0) {
		return migrate_key(argc, argv);
	}
	if (std::strcmp(command, "refresh") == 0) {
		return refresh(argc, argv);
	}
	// Plumbing commands (executed by git, not by user):
	if (std::strcmp(command, "clean") == 0) {
		return clean(argc, argv);
	}
	if (std::strcmp(command, "smudge") == 0) {
		return smudge(argc, argv);
	}
	if (std::strcmp(command, "diff") == 0) {
		return diff(argc, argv);
	}

	print_usage(std::clog);
	return 2;

} catch (const Error& e) {
	std::cerr << "git-crypt: Error: " << e.message << std::endl;
	return 1;
} catch (const Gpg_error& e) {
	std::cerr << "git-crypt: GPG error: " << e.message << std::endl;
	return 1;
} catch (const System_error& e) {
	std::cerr << "git-crypt: " << e.action << ": ";
	if (!e.target.empty()) {
		std::cerr << e.target << ": ";
	}
	std::cerr << strerror(e.error) << std::endl;
	return 1;
} catch (const Crypto_error& e) {
	std::cerr << "git-crypt: Crypto error: " << e.where << ": " << e.message << std::endl;
	return 1;
} catch (Key_file::Incompatible) {
	std::cerr << "git-crypt: This repository contains a incompatible key file.  Please upgrade git-crypt." << std::endl;
	return 1;
} catch (Key_file::Malformed) {
	std::cerr << "git-crypt: This repository contains a malformed key file.  It may be corrupted." << std::endl;
	return 1;
} catch (const std::ios_base::failure& e) {
	std::cerr << "git-crypt: I/O error: " << e.what() << std::endl;
	return 1;
}


