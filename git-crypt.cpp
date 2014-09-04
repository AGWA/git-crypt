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
#include "parse_options.hpp"
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <string.h>

const char*	argv0;

static void print_usage (std::ostream& out)
{
	out << "Usage: " << argv0 << " COMMAND [ARGS ...]" << std::endl;
	out << std::endl;
	//     |--------------------------------------------------------------------------------| 80 characters
	out << "Common commands:" << std::endl;
	out << "   init                generate a key and prepare repo to use git-crypt" << std::endl;
	out << "   status              display which files are encrypted" << std::endl;
	//out << "   refresh             ensure all files in the repo are properly decrypted" << std::endl;
	out << std::endl;
	out << "GPG commands:" << std::endl;
	out << "   add-gpg-key KEYID   add the user with the given GPG key ID as a collaborator" << std::endl;
	//out << "   rm-gpg-key KEYID    revoke collaborator status from the given GPG key ID" << std::endl;
	//out << "   ls-gpg-keys         list the GPG key IDs of collaborators" << std::endl;
	out << "   unlock              decrypt this repo using the in-repo GPG-encrypted key" << std::endl;
	out << "   lock                check out encrypted versions of files in this repo" << std::endl;
	out << std::endl;
	out << "Symmetric key commands:" << std::endl;
	out << "   export-key FILE     export this repo's symmetric key to the given file" << std::endl;
	out << "   unlock KEYFILE      decrypt this repo using the given symmetric key" << std::endl;
	out << std::endl;
	out << "Legacy commands:" << std::endl;
	out << "   init KEYFILE        alias for 'unlock KEYFILE'" << std::endl;
	out << "   keygen KEYFILE      generate a git-crypt key in the given file" << std::endl;
	out << "   migrate-key FILE    migrate the given legacy key file to the latest format" << std::endl;
	/*
	out << std::endl;
	out << "Plumbing commands (not to be used directly):" << std::endl;
	out << "   clean [LEGACY-KEYFILE]" << std::endl;
	out << "   smudge [LEGACY-KEYFILE]" << std::endl;
	out << "   diff [LEGACY-KEYFILE] FILE" << std::endl;
	*/
	/*
	out << std::endl;
	out << "See 'git-crypt help COMMAND' for more information on a specific command." << std::endl;
	*/
}


int main (int argc, const char** argv)
try {
	argv0 = argv[0];

	/*
	 * General initialization
	 */

	init_std_streams();
	init_crypto();

	/*
	 * Parse command line arguments
	 */
	int			arg_index = 1;
	while (arg_index < argc && argv[arg_index][0] == '-') {
		if (std::strcmp(argv[arg_index], "--help") == 0) {
			print_usage(std::clog);
			return 0;
		} else if (std::strcmp(argv[arg_index], "--") == 0) {
			++arg_index;
			break;
		} else {
			std::clog << argv0 << ": " << argv[arg_index] << ": Unknown option" << std::endl;
			print_usage(std::clog);
			return 2;
		}
	}

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
	if (std::strcmp(command, "lock") == 0) {
		return lock(argc, argv);
	}
	if (std::strcmp(command, "add-gpg-key") == 0) {
		return add_gpg_key(argc, argv);
	}
	if (std::strcmp(command, "rm-gpg-key") == 0) {
		return rm_gpg_key(argc, argv);
	}
	if (std::strcmp(command, "ls-gpg-keys") == 0) {
		return ls_gpg_keys(argc, argv);
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
	if (std::strcmp(command, "status") == 0) {
		return status(argc, argv);
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
	std::cerr << "git-crypt: System error: " << e.message() << std::endl;
	return 1;
} catch (const Crypto_error& e) {
	std::cerr << "git-crypt: Crypto error: " << e.where << ": " << e.message << std::endl;
	return 1;
} catch (const Option_error& e) {
	std::cerr << "git-crypt: Error: " << e.option_name << ": " << e.message << std::endl;
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


