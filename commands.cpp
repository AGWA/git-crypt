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

#include "commands.hpp"
#include "crypto.hpp"
#include "util.hpp"
#include "key.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstddef>
#include <cstring>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static void configure_git_filters ()
{
	std::string	git_crypt_path(our_exe_path());

	// git config filter.git-crypt.smudge "/path/to/git-crypt smudge"
	std::string	command("git config filter.git-crypt.smudge ");
	command += escape_shell_arg(escape_shell_arg(git_crypt_path) + " smudge");

	if (!successful_exit(system(command.c_str()))) {
		throw Error("'git config' failed");
	}

	// git config filter.git-crypt.clean "/path/to/git-crypt clean"
	command = "git config filter.git-crypt.clean ";
	command += escape_shell_arg(escape_shell_arg(git_crypt_path) + " clean");

	if (!successful_exit(system(command.c_str()))) {
		throw Error("'git config' failed");
	}

	// git config diff.git-crypt.textconv "/path/to/git-crypt diff"
	command = "git config diff.git-crypt.textconv ";
	command += escape_shell_arg(escape_shell_arg(git_crypt_path) + " diff");

	if (!successful_exit(system(command.c_str()))) {
		throw Error("'git config' failed");
	}
}

static std::string get_internal_key_path ()
{
	std::stringstream	output;

	if (!successful_exit(exec_command("git rev-parse --git-dir", output))) {
		throw Error("'git rev-parse --git-dir' - is this a Git repository?");
	}

	std::string		path;
	std::getline(output, path);
	path += "/git-crypt/key";
	return path;
}

static void load_key (Key_file& key_file, const char* legacy_path =0)
{
	if (legacy_path) {
		std::ifstream		key_file_in(legacy_path, std::fstream::binary);
		if (!key_file_in) {
			throw Error(std::string("Unable to open key file: ") + legacy_path);
		}
		key_file.load_legacy(key_file_in);
	} else {
		std::ifstream		key_file_in(get_internal_key_path().c_str(), std::fstream::binary);
		if (!key_file_in) {
			throw Error("Unable to open key file - have you unlocked/initialized this repository yet?");
		}
		key_file.load(key_file_in);
	}
}


// Encrypt contents of stdin and write to stdout
int clean (int argc, char** argv)
{
	const char*	legacy_key_path = 0;
	if (argc == 0) {
	} else if (argc == 1) {
		legacy_key_path = argv[0];
	} else {
		std::clog << "Usage: git-crypt smudge" << std::endl;
		return 2;
	}
	Key_file		key_file;
	load_key(key_file, legacy_key_path);

	const Key_file::Entry*	key = key_file.get_latest();
	if (!key) {
		std::clog << "git-crypt: error: key file is empty" << std::endl;
		return 1;
	}

	// Read the entire file

	Hmac_sha1_state	hmac(key->hmac_key, HMAC_KEY_LEN); // Calculate the file's SHA1 HMAC as we go
	uint64_t		file_size = 0;	// Keep track of the length, make sure it doesn't get too big
	std::string		file_contents;	// First 8MB or so of the file go here
	std::fstream		temp_file;	// The rest of the file spills into a temporary file on disk
	temp_file.exceptions(std::fstream::badbit);

	char			buffer[1024];

	while (std::cin && file_size < Aes_ctr_encryptor::MAX_CRYPT_BYTES) {
		std::cin.read(buffer, sizeof(buffer));

		size_t	bytes_read = std::cin.gcount();

		hmac.add(reinterpret_cast<unsigned char*>(buffer), bytes_read);
		file_size += bytes_read;

		if (file_size <= 8388608) {
			file_contents.append(buffer, bytes_read);
		} else {
			if (!temp_file.is_open()) {
				open_tempfile(temp_file, std::fstream::in | std::fstream::out | std::fstream::binary | std::fstream::app);
			}
			temp_file.write(buffer, bytes_read);
		}
	}

	// Make sure the file isn't so large we'll overflow the counter value (which would doom security)
	if (file_size >= Aes_ctr_encryptor::MAX_CRYPT_BYTES) {
		std::clog << "git-crypt: error: file too long to encrypt securely" << std::endl;
		return 1;
	}

	// We use an HMAC of the file as the encryption nonce (IV) for CTR mode.
	// By using a hash of the file we ensure that the encryption is
	// deterministic so git doesn't think the file has changed when it really
	// hasn't.  CTR mode with a synthetic IV is provably semantically secure
	// under deterministic CPA as long as the synthetic IV is derived from a
	// secure PRF applied to the message.  Since HMAC-SHA1 is a secure PRF, this
	// encryption scheme is semantically secure under deterministic CPA.
	// 
	// Informally, consider that if a file changes just a tiny bit, the IV will
	// be completely different, resulting in a completely different ciphertext
	// that leaks no information about the similarities of the plaintexts.  Also,
	// since we're using the output from a secure hash function plus a counter
	// as the input to our block cipher, we should never have a situation where
	// two different plaintext blocks get encrypted with the same CTR value.  A
	// nonce will be reused only if the entire file is the same, which leaks no
	// information except that the files are the same.
	//
	// To prevent an attacker from building a dictionary of hash values and then
	// looking up the nonce (which must be stored in the clear to allow for
	// decryption), we use an HMAC as opposed to a straight hash.

	// Note: Hmac_sha1_state::LEN >= Aes_ctr_encryptor::NONCE_LEN

	unsigned char		digest[Hmac_sha1_state::LEN];
	hmac.get(digest);

	// Write a header that...
	std::cout.write("\0GITCRYPT\0", 10); // ...identifies this as an encrypted file
	std::cout.write(reinterpret_cast<char*>(digest), Aes_ctr_encryptor::NONCE_LEN); // ...includes the nonce

	// Now encrypt the file and write to stdout
	Aes_ctr_encryptor	aes(key->aes_key, digest);

	// First read from the in-memory copy
	const unsigned char*	file_data = reinterpret_cast<const unsigned char*>(file_contents.data());
	size_t			file_data_len = file_contents.size();
	while (file_data_len > 0) {
		size_t		buffer_len = std::min(sizeof(buffer), file_data_len);
		aes.process(file_data, reinterpret_cast<unsigned char*>(buffer), buffer_len);
		std::cout.write(buffer, buffer_len);
		file_data += buffer_len;
		file_data_len -= buffer_len;
	}

	// Then read from the temporary file if applicable
	if (temp_file.is_open()) {
		temp_file.seekg(0);
		while (temp_file.peek() != -1) {
			temp_file.read(buffer, sizeof(buffer));

			size_t	buffer_len = temp_file.gcount();

			aes.process(reinterpret_cast<unsigned char*>(buffer),
			            reinterpret_cast<unsigned char*>(buffer),
			            buffer_len);
			std::cout.write(buffer, buffer_len);
		}
	}

	return 0;
}

// Decrypt contents of stdin and write to stdout
int smudge (int argc, char** argv)
{
	const char*	legacy_key_path = 0;
	if (argc == 0) {
	} else if (argc == 1) {
		legacy_key_path = argv[0];
	} else {
		std::clog << "Usage: git-crypt smudge" << std::endl;
		return 2;
	}
	Key_file		key_file;
	load_key(key_file, legacy_key_path);

	// Read the header to get the nonce and make sure it's actually encrypted
	unsigned char		header[10 + Aes_ctr_decryptor::NONCE_LEN];
	std::cin.read(reinterpret_cast<char*>(header), sizeof(header));
	if (!std::cin || std::cin.gcount() != sizeof(header) || std::memcmp(header, "\0GITCRYPT\0", 10) != 0) {
		std::clog << "git-crypt: error: file not encrypted" << std::endl;
		return 1;
	}
	const unsigned char*	nonce = header + 10;
	uint32_t		key_version = 0; // TODO: get the version from the file header

	const Key_file::Entry*	key = key_file.get(key_version);
	if (!key) {
		std::clog << "git-crypt: error: key version " << key_version << " not available - please unlock with the latest version of the key." << std::endl;
		return 1;
	}

	Aes_ctr_decryptor::process_stream(std::cin, std::cout, key->aes_key, nonce);
	return 0;
}

int diff (int argc, char** argv)
{
	const char*	filename = 0;
	const char*	legacy_key_path = 0;
	if (argc == 1) {
		filename = argv[0];
	} else if (argc == 2) {
		legacy_key_path = argv[0];
		filename = argv[1];
	} else {
		std::clog << "Usage: git-crypt diff FILENAME" << std::endl;
		return 2;
	}
	Key_file		key_file;
	load_key(key_file, legacy_key_path);

	// Open the file
	std::ifstream		in(filename, std::fstream::binary);
	if (!in) {
		std::clog << "git-crypt: " << filename << ": unable to open for reading" << std::endl;
		return 1;
	}
	in.exceptions(std::fstream::badbit);

	// Read the header to get the nonce and determine if it's actually encrypted
	unsigned char		header[10 + Aes_ctr_decryptor::NONCE_LEN];
	in.read(reinterpret_cast<char*>(header), sizeof(header));
	if (!in || in.gcount() != sizeof(header) || std::memcmp(header, "\0GITCRYPT\0", 10) != 0) {
		// File not encrypted - just copy it out to stdout
		std::cout.write(reinterpret_cast<char*>(header), in.gcount()); // don't forget to include the header which we read!
		std::cout << in.rdbuf();
		return 0;
	}

	// Go ahead and decrypt it
	const unsigned char*	nonce = header + 10;
	uint32_t		key_version = 0; // TODO: get the version from the file header

	const Key_file::Entry*	key = key_file.get(key_version);
	if (!key) {
		std::clog << "git-crypt: error: key version " << key_version << " not available - please unlock with the latest version of the key." << std::endl;
		return 1;
	}

	Aes_ctr_decryptor::process_stream(in, std::cout, key->aes_key, nonce);
	return 0;
}

int init (int argc, char** argv)
{
	if (argc == 1) {
		std::clog << "Warning: 'git-crypt init' with a key file is deprecated as of git-crypt 0.4" << std::endl;
		std::clog << "and will be removed in a future release. Please get in the habit of using" << std::endl;
		std::clog << "'git-crypt unlock KEYFILE' instead." << std::endl;
		return unlock(argc, argv);
	}
	if (argc != 0) {
		std::clog << "Error: 'git-crypt init' takes no arguments." << std::endl;
		return 2;
	}

	std::string		internal_key_path(get_internal_key_path());
	if (access(internal_key_path.c_str(), F_OK) == 0) {
		// TODO: add a -f option to reinitialize the repo anyways (this should probably imply a refresh)
		std::clog << "Error: this repository has already been initialized with git-crypt." << std::endl;
		return 1;
	}

	// 1. Generate a key and install it
	std::clog << "Generating key..." << std::endl;
	Key_file		key_file;
	key_file.generate();

	mkdir_parent(internal_key_path);
	if (!key_file.store_to_file(internal_key_path.c_str())) {
		std::clog << "Error: " << internal_key_path << ": unable to write key file" << std::endl;
		return 1;
	}

	// 2. Configure git for git-crypt
	configure_git_filters();

	return 0;
}

int unlock (int argc, char** argv)
{
	const char*		symmetric_key_file = 0;
	if (argc == 0) {
	} else if (argc == 1) {
		symmetric_key_file = argv[0];
	} else {
		std::clog << "Usage: git-crypt unlock [KEYFILE]" << std::endl;
		return 2;
	}

	// 0. Check to see if HEAD exists.  See below why we do this.
	bool			head_exists = successful_exit(system("git rev-parse HEAD >/dev/null 2>/dev/null"));

	// 1. Make sure working directory is clean (ignoring untracked files)
	// We do this because we run 'git checkout -f HEAD' later and we don't
	// want the user to lose any changes.  'git checkout -f HEAD' doesn't touch
	// untracked files so it's safe to ignore those.
	int			status;
	std::stringstream	status_output;
	status = exec_command("git status -uno --porcelain", status_output);
	if (!successful_exit(status)) {
		std::clog << "Error: 'git status' failed - is this a git repository?" << std::endl;
		return 1;
	} else if (status_output.peek() != -1 && head_exists) {
		// We only care that the working directory is dirty if HEAD exists.
		// If HEAD doesn't exist, we won't be resetting to it (see below) so
		// it doesn't matter that the working directory is dirty.
		std::clog << "Error: Working directory not clean." << std::endl;
		std::clog << "Please commit your changes or 'git stash' them before running 'git-crypt' unlock." << std::endl;
		return 1;
	}

	// 2. Determine the path to the top of the repository.  We pass this as the argument
	// to 'git checkout' below. (Determine the path now so in case it fails we haven't already
	// mucked with the git config.)
	std::stringstream	cdup_output;
	if (!successful_exit(exec_command("git rev-parse --show-cdup", cdup_output))) {
		std::clog << "Error: 'git rev-parse --show-cdup' failed" << std::endl;
		return 1;
	}

	// 3. Install the key
	Key_file		key_file;
	if (symmetric_key_file) {
		// Read from the symmetric key file
		try {
			if (std::strcmp(symmetric_key_file, "-") == 0) {
				key_file.load(std::cin);
			} else {
				if (!key_file.load_from_file(symmetric_key_file)) {
					std::clog << "Error: " << symmetric_key_file << ": unable to read key file" << std::endl;
					return 1;
				}
			}
		} catch (Key_file::Incompatible) {
			std::clog << "Error: " << symmetric_key_file << " is in an incompatible format" << std::endl;
			std::clog << "Please upgrade to a newer version of git-crypt." << std::endl;
			return 1;
		} catch (Key_file::Malformed) {
			std::clog << "Error: " << symmetric_key_file << ": not a valid git-crypt key file" << std::endl;
			std::clog << "If this key was created prior to git-crypt 0.4, you need to migrate it" << std::endl;
			std::clog << "by running 'git-crypt migrate-key /path/to/key/file'." << std::endl;
			return 1;
		}
	} else {
		// Decrypt GPG key from root of repo (TODO NOW)
		std::clog << "Error: GPG support is not yet implemented" << std::endl;
		return 1;
	}
	std::string		internal_key_path(get_internal_key_path());
	// TODO: croak if internal_key_path already exists???
	mkdir_parent(internal_key_path);
	if (!key_file.store_to_file(internal_key_path.c_str())) {
		std::clog << "Error: " << internal_key_path << ": unable to write key file" << std::endl;
		return 1;
	}

	// 4. Configure git for git-crypt
	configure_git_filters();

	// 5. Do a force checkout so any files that were previously checked out encrypted
	//    will now be checked out decrypted.
	// If HEAD doesn't exist (perhaps because this repo doesn't have any files yet)
	// just skip the checkout.
	if (head_exists) {
		std::string	path_to_top;
		std::getline(cdup_output, path_to_top);

		std::string	command("git checkout -f HEAD -- ");
		if (path_to_top.empty()) {
			command += ".";
		} else {
			command += escape_shell_arg(path_to_top);
		}

		if (!successful_exit(system(command.c_str()))) {
			std::clog << "Error: 'git checkout' failed" << std::endl;
			std::clog << "git-crypt has been set up but existing encrypted files have not been decrypted" << std::endl;
			return 1;
		}
	}

	return 0;
}

int add_collab (int argc, char** argv) // TODO NOW
{
	// Sketch:
	// 1. Resolve the key ID to a long hex ID
	// 2. Create the in-repo key directory if it doesn't exist yet.
	// 3. For most recent key version KEY_VERSION (or for each key version KEY_VERSION if retroactive option specified):
	//     Encrypt KEY_VERSION with the GPG key and stash it in .git-crypt/keys/KEY_VERSION/LONG_HEX_ID
	//      if file already exists, print a notice and move on
	// 4. Commit the new file(s) (if any) with a meanignful commit message, unless -n was passed
	std::clog << "Error: add-collab is not yet implemented." << std::endl;
	return 1;
}

int rm_collab (int argc, char** argv) // TODO
{
	std::clog << "Error: rm-collab is not yet implemented." << std::endl;
	return 1;
}

int ls_collabs (int argc, char** argv) // TODO
{
	// Sketch:
	// Scan the sub-directories in .git-crypt/keys, outputting something like this:
	// ====
	// Key version 0:
	//  0x143DE9B3F7316900 Andrew Ayer <andrew@example.com>
	//  0x4E386D9C9C61702F ???
	// Key version 1:
	//  0x143DE9B3F7316900 Andrew Ayer <andrew@example.com>
	//  0x1727274463D27F40 John Smith <smith@example.com>
	//  0x4E386D9C9C61702F ???
	// ====
	// To resolve a long hex ID, use a command like this:
	//  gpg --options /dev/null --fixed-list-mode --batch --with-colons --list-keys 0x143DE9B3F7316900

	std::clog << "Error: ls-collabs is not yet implemented." << std::endl;
	return 1;
}

int export_key (int argc, char** argv)
{
	// TODO: provide options to export only certain key versions

	if (argc != 1) {
		std::clog << "Usage: git-crypt export-key FILENAME" << std::endl;
		return 2;
	}

	Key_file		key_file;
	load_key(key_file);

	const char*		out_file_name = argv[0];

	if (std::strcmp(out_file_name, "-") == 0) {
		key_file.store(std::cout);
	} else {
		if (!key_file.store_to_file(out_file_name)) {
			std::clog << "Error: " << out_file_name << ": unable to write key file" << std::endl;
			return 1;
		}
	}

	return 0;
}

int keygen (int argc, char** argv)
{
	if (argc != 1) {
		std::clog << "Usage: git-crypt keygen KEYFILE" << std::endl;
		return 2;
	}

	const char*		key_file_name = argv[0];

	if (std::strcmp(key_file_name, "-") != 0 && access(key_file_name, F_OK) == 0) {
		std::clog << key_file_name << ": File already exists" << std::endl;
		return 1;
	}

	std::clog << "Generating key..." << std::endl;
	Key_file		key_file;
	key_file.generate();

	if (std::strcmp(key_file_name, "-") == 0) {
		key_file.store(std::cout);
	} else {
		if (!key_file.store_to_file(key_file_name)) {
			std::clog << "Error: " << key_file_name << ": unable to write key file" << std::endl;
			return 1;
		}
	}
	return 0;
}

int migrate_key (int argc, char** argv)
{
	if (argc != 1) {
		std::clog << "Usage: git-crypt migrate-key KEYFILE" << std::endl;
		return 2;
	}

	const char*		key_file_name = argv[0];
	Key_file		key_file;

	try {
		if (std::strcmp(key_file_name, "-") == 0) {
			key_file.load_legacy(std::cin);
			key_file.store(std::cout);
		} else {
			std::ifstream	in(key_file_name, std::fstream::binary);
			if (!in) {
				std::clog << "Error: " << key_file_name << ": unable to open for reading" << std::endl;
				return 1;
			}
			key_file.load_legacy(in);
			in.close();

			std::string	new_key_file_name(key_file_name);
			new_key_file_name += ".new";

			if (access(new_key_file_name.c_str(), F_OK) == 0) {
				std::clog << new_key_file_name << ": File already exists" << std::endl;
				return 1;
			}

			if (!key_file.store_to_file(new_key_file_name.c_str())) {
				std::clog << "Error: " << new_key_file_name << ": unable to write key file" << std::endl;
				return 1;
			}

			if (rename(new_key_file_name.c_str(), key_file_name) == -1) {
				std::clog << "Error: " << key_file_name << ": " << strerror(errno) << std::endl;
				unlink(new_key_file_name.c_str());
				return 1;
			}
		}
	} catch (Key_file::Malformed) {
		std::clog << "Error: " << key_file_name << ": not a valid legacy git-crypt key file" << std::endl;
		return 1;
	}

	return 0;
}

int refresh (int argc, char** argv) // TODO: do a force checkout, much like in unlock
{
	std::clog << "Error: refresh is not yet implemented." << std::endl;
	return 1;
}

