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
#include "crypto.hpp"
#include "util.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <algorithm>
#include <string>
#include <fstream>
#include <iostream>
#include <cstddef>
#include <cstring>

// Encrypt contents of stdin and write to stdout
void clean (const char* keyfile)
{
	keys_t		keys;
	load_keys(keyfile, &keys);

	// Read the entire file

	hmac_sha1_state	hmac(keys.hmac, HMAC_KEY_LEN);	// Calculate the file's SHA1 HMAC as we go
	uint64_t	file_size = 0;	// Keep track of the length, make sure it doesn't get too big
	std::string	file_contents;	// First 8MB or so of the file go here
	std::fstream	temp_file;	// The rest of the file spills into a temporary file on disk
	temp_file.exceptions(std::fstream::badbit);

	char		buffer[1024];

	while (std::cin && file_size < MAX_CRYPT_BYTES) {
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
	if (file_size >= MAX_CRYPT_BYTES) {
		std::clog << "File too long to encrypt securely\n";
		std::exit(1);
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

	uint8_t		digest[SHA1_LEN];
	hmac.get(digest);

	// Write a header that...
	std::cout.write("\0GITCRYPT\0", 10); // ...identifies this as an encrypted file
	std::cout.write(reinterpret_cast<char*>(digest), NONCE_LEN); // ...includes the nonce

	// Now encrypt the file and write to stdout
	aes_ctr_state	state(digest, NONCE_LEN);

	// First read from the in-memory copy
	const uint8_t*	file_data = reinterpret_cast<const uint8_t*>(file_contents.data());
	size_t		file_data_len = file_contents.size();
	for (size_t i = 0; i < file_data_len; i += sizeof(buffer)) {
		size_t	buffer_len = std::min(sizeof(buffer), file_data_len - i);
		state.process(&keys.enc, file_data + i, reinterpret_cast<uint8_t*>(buffer), buffer_len);
		std::cout.write(buffer, buffer_len);
	}

	// Then read from the temporary file if applicable
	if (temp_file.is_open()) {
		temp_file.seekg(0);
		while (temp_file) {
			temp_file.read(buffer, sizeof(buffer));

			size_t buffer_len = temp_file.gcount();

			state.process(&keys.enc, reinterpret_cast<uint8_t*>(buffer), reinterpret_cast<uint8_t*>(buffer), buffer_len);
			std::cout.write(buffer, buffer_len);
		}
	}
}

// Decrypt contents of stdin and write to stdout
void smudge (const char* keyfile)
{
	keys_t		keys;
	load_keys(keyfile, &keys);

	// Read the header to get the nonce and make sure it's actually encrypted
	char		header[22];
	std::cin.read(header, 22);
	if (!std::cin || std::cin.gcount() != 22 || memcmp(header, "\0GITCRYPT\0", 10) != 0) {
		std::clog << "File not encrypted\n";
		std::exit(1);
	}

	process_stream(std::cin, std::cout, &keys.enc, reinterpret_cast<uint8_t*>(header + 10));
}

void diff (const char* keyfile, const char* filename)
{
	keys_t		keys;
	load_keys(keyfile, &keys);

	// Open the file
	std::ifstream	in(filename);
	if (!in) {
		perror(filename);
		std::exit(1);
	}
	in.exceptions(std::fstream::badbit);

	// Read the header to get the nonce and determine if it's actually encrypted
	char		header[22];
	in.read(header, 22);
	if (!in || in.gcount() != 22 || memcmp(header, "\0GITCRYPT\0", 10) != 0) {
		// File not encrypted - just copy it out to stdout
		std::cout.write(header, in.gcount()); // don't forget to include the header which we read!
		char	buffer[1024];
		while (in) {
			in.read(buffer, sizeof(buffer));
			std::cout.write(buffer, in.gcount());
		}
		return;
	}

	process_stream(in, std::cout, &keys.enc, reinterpret_cast<uint8_t*>(header + 10));
}


void init (const char* argv0, const char* keyfile)
{
	if (access(keyfile, R_OK) == -1) {
		perror(keyfile);
		std::exit(1);
	}
	
	// 0. Check to see if HEAD exists.  See below why we do this.
	bool		head_exists = system("git rev-parse HEAD >/dev/null 2>/dev/null") == 0;

	// 1. Make sure working directory is clean (ignoring untracked files)
	// We do this because we run 'git reset --hard HEAD' later and we don't
	// want the user to lose any changes.  'git reset' doesn't touch
	// untracked files so it's safe to ignore those.
	int		status;
	std::string	status_output;
	status = exec_command("git status -uno --porcelain", status_output);
	if (status != 0) {
		std::clog << "git status failed - is this a git repository?\n";
		std::exit(1);
	} else if (!status_output.empty() && head_exists) {
		// We only care that the working directory is dirty if HEAD exists.
		// If HEAD doesn't exist, we won't be resetting to it (see below) so
		// it doesn't matter that the working directory is dirty.
		std::clog << "Working directory not clean.\n";
		std::clog << "Please commit your changes or 'git stash' them before setting up git-crypt.\n";
		std::exit(1);
	}

	std::string	git_crypt_path(std::strchr(argv0, '/') ? resolve_path(argv0) : argv0);
	std::string	keyfile_path(resolve_path(keyfile));


	// 2. Add config options to git

	// git config filter.git-crypt.smudge "git-crypt smudge /path/to/key"
	std::string	command("git config filter.git-crypt.smudge \"");
	command += git_crypt_path;
	command += " smudge ";
	command += keyfile_path;
	command += "\"";
	
	if (system(command.c_str()) != 0) {
		std::clog << "git config failed\n";
		std::exit(1);
	}

	// git config filter.git-crypt.clean "git-crypt clean /path/to/key"
	command = "git config filter.git-crypt.clean \"";
	command += git_crypt_path;
	command += " clean ";
	command += keyfile_path;
	command += "\"";
	
	if (system(command.c_str()) != 0) {
		std::clog << "git config failed\n";
		std::exit(1);
	}

	// git config diff.git-crypt.textconv "git-crypt diff /path/to/key"
	command = "git config diff.git-crypt.textconv \"";
	command += git_crypt_path;
	command += " diff ";
	command += keyfile_path;
	command += "\"";
	
	if (system(command.c_str()) != 0) {
		std::clog << "git config failed\n";
		std::exit(1);
	}


	// 3. Do a hard reset so any files that were previously checked out encrypted
	//    will now be checked out decrypted.
	// If HEAD doesn't exist (perhaps because this repo doesn't have any files yet)
	// just skip the reset.
	if (head_exists && system("git reset --hard HEAD") != 0) {
		std::clog << "git reset --hard failed\n";
		std::exit(1);
	}
}

void keygen (const char* keyfile)
{
	mode_t		old_umask = umask(0077); // make sure key file is protected
	std::ofstream	keyout(keyfile);
	if (!keyout) {
		perror(keyfile);
		std::exit(1);
	}
	umask(old_umask);
	std::ifstream	randin("/dev/random");
	if (!randin) {
		perror("/dev/random");
		std::exit(1);
	}
	char		buffer[AES_KEY_BITS/8 + HMAC_KEY_LEN];
	randin.read(buffer, sizeof(buffer));
	if (randin.gcount() != sizeof(buffer)) {
		std::clog << "Premature end of random data.\n";
		std::exit(1);
	}
	keyout.write(buffer, sizeof(buffer));
}
