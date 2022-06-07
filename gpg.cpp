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

#include "gpg.hpp"
#include "util.hpp"
#include "commands.hpp"
#include <sstream>

static std::string gpg_get_executable()
{
	std::string gpgbin = "gpg";
	try {
		gpgbin = get_git_config("gpg.program");
	} catch (...) {
	}
	return gpgbin;
}
static std::string gpg_nth_column (const std::string& line, unsigned int col)
{
	std::string::size_type	pos = 0;

	for (unsigned int i = 0; i < col; ++i) {
		pos = line.find_first_of(':', pos);
		if (pos == std::string::npos) {
			throw Gpg_error("Malformed output from gpg");
		}
		pos = pos + 1;
	}

	const std::string::size_type	end_pos = line.find_first_of(':', pos);

	return end_pos != std::string::npos ?
	       line.substr(pos, end_pos - pos) :
	       line.substr(pos);
}

// given a key fingerprint, return the key's UID (e.g. "John Smith <jsmith@example.com>")
std::string gpg_get_uid (const std::string& fingerprint)
{
	// gpg --batch --with-colons --fixed-list-mode --list-keys 0x7A399B2DB06D039020CD1CE1D0F3702D61489532
	std::vector<std::string>	command;
	command.push_back(gpg_get_executable());
	command.push_back("--batch");
	command.push_back("--with-colons");
	command.push_back("--fixed-list-mode");
	command.push_back("--list-keys");
	command.push_back("0x" + fingerprint);
	std::stringstream		command_output;
	if (!successful_exit(exec_command(command, command_output))) {
		// This could happen if the keyring does not contain a public key with this fingerprint
		return "";
	}

	while (command_output.peek() != -1) {
		std::string		line;
		std::getline(command_output, line);
		if (line.substr(0, 4) == "uid:") {
			// uid:u::::1395975462::AB97D6E3E5D8789988CA55E5F77D9E7397D05229::John Smith <jsmith@example.com>:
			// want the 9th column (counting from 0)
			return gpg_nth_column(line, 9);
		}
	}
	
	return "";
}

// return a list of fingerprints of public keys matching the given search query (such as jsmith@example.com)
std::vector<std::string> gpg_lookup_key (const std::string& query)
{
	std::vector<std::string>	fingerprints;

	// gpg --batch --with-colons --fingerprint --list-keys jsmith@example.com
	std::vector<std::string>	command;
	command.push_back(gpg_get_executable());
	command.push_back("--batch");
	command.push_back("--with-colons");
	command.push_back("--fingerprint");
	command.push_back("--list-keys");
	command.push_back(query);
	std::stringstream		command_output;
	if (successful_exit(exec_command(command, command_output))) {
		bool			is_pubkey = false;
		while (command_output.peek() != -1) {
			std::string		line;
			std::getline(command_output, line);
			if (line.substr(0, 4) == "pub:") {
				is_pubkey = true;
			} else if (line.substr(0, 4) == "sub:") {
				is_pubkey = false;
			} else if (is_pubkey && line.substr(0, 4) == "fpr:") {
				// fpr:::::::::7A399B2DB06D039020CD1CE1D0F3702D61489532:
				// want the 9th column (counting from 0)
				fingerprints.push_back(gpg_nth_column(line, 9));
			}
		}
	}
	
	return fingerprints;
}

std::vector<std::string> gpg_list_secret_keys ()
{
	// gpg --batch --with-colons --list-secret-keys --fingerprint
	std::vector<std::string>	command;
	command.push_back(gpg_get_executable());
	command.push_back("--batch");
	command.push_back("--with-colons");
	command.push_back("--list-secret-keys");
	command.push_back("--fingerprint");
	std::stringstream		command_output;
	if (!successful_exit(exec_command(command, command_output))) {
		throw Gpg_error("gpg --list-secret-keys failed");
	}

	std::vector<std::string>	secret_keys;

	while (command_output.peek() != -1) {
		std::string		line;
		std::getline(command_output, line);
		if (line.substr(0, 4) == "fpr:") {
			// fpr:::::::::7A399B2DB06D039020CD1CE1D0F3702D61489532:
			// want the 9th column (counting from 0)
			secret_keys.push_back(gpg_nth_column(line, 9));
		}
	}
	
	return secret_keys;
}

void gpg_encrypt_to_file (const std::string& filename, const std::string& recipient_fingerprint, bool key_is_trusted, const char* p, size_t len)
{
	// gpg --batch -o FILENAME -r RECIPIENT -e
	std::vector<std::string>	command;
	command.push_back(gpg_get_executable());
	command.push_back("--batch");
	if (key_is_trusted) {
		command.push_back("--trust-model");
		command.push_back("always");
	}
	command.push_back("-o");
	command.push_back(filename);
	command.push_back("-r");
	command.push_back("0x" + recipient_fingerprint);
	command.push_back("-e");
	if (!successful_exit(exec_command_with_input(command, p, len))) {
		throw Gpg_error("Failed to encrypt");
	}
}

void gpg_decrypt_from_file (const std::string& filename, std::ostream& output)
{
	// gpg -q -d FILENAME
	std::vector<std::string>	command;
	command.push_back(gpg_get_executable());
	command.push_back("-q");
	command.push_back("-d");
	command.push_back(filename);
	if (!successful_exit(exec_command(command, output))) {
		throw Gpg_error("Failed to decrypt");
	}
}

