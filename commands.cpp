#include "commands.hpp"
#include "crypto.hpp"
#include "util.hpp"
#include <sys/types.h>
#include <sys/stat.h>
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

	// First read the entire file into a buffer (TODO: if the buffer gets big, use a temp file instead)
	std::string	file_contents;
	char		buffer[1024];
	while (std::cin) {
		std::cin.read(buffer, sizeof(buffer));
		file_contents.append(buffer, std::cin.gcount());
	}
	const uint8_t*	file_data = reinterpret_cast<const uint8_t*>(file_contents.data());
	size_t		file_len = file_contents.size();

	// Make sure the file isn't so large we'll overflow the counter value (which would doom security)
	if (file_len > MAX_CRYPT_BYTES) {
		std::clog << "File too long to encrypt securely\n";
		std::exit(1);
	}

	// Compute an HMAC of the file to use as the encryption nonce (IV) for CTR
	// mode.  By using a hash of the file we ensure that the encryption is
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
	uint8_t		digest[12];
	hmac_sha1_96(digest, file_data, file_len, keys.hmac, HMAC_KEY_LEN);

	// Write a header that...
	std::cout.write("\0GITCRYPT\0", 10); // ...identifies this as an encrypted file
	std::cout.write(reinterpret_cast<char*>(digest), 12); // ...includes the nonce

	// Now encrypt the file and write to stdout
	aes_ctr_state	state(digest, 12);
	for (size_t i = 0; i < file_len; i += sizeof(buffer)) {
		size_t	block_len = std::min(sizeof(buffer), file_len - i);
		state.process_block(&keys.enc, file_data + i, reinterpret_cast<uint8_t*>(buffer), block_len);
		std::cout.write(buffer, block_len);
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

	// 1. Make sure working directory is clean
	int		status;
	std::string	status_output;
	status = exec_command("git status --porcelain", status_output);
	if (status != 0) {
		std::clog << "git status failed - is this a git repository?\n";
		std::exit(1);
	} else if (!status_output.empty()) {
		std::clog << "Working directory not clean.\n";
		std::exit(1);
	}

	std::string	git_crypt_path(std::strchr(argv0, '/') ? resolve_path(argv0) : argv0);
	std::string	keyfile_path(resolve_path(keyfile));


	// 2. Add config options to git

	// git config --add filter.git-crypt.smudge "git-crypt smudge /path/to/key"
	std::string	command("git config --add filter.git-crypt.smudge \"");
	command += git_crypt_path;
	command += " smudge ";
	command += keyfile_path;
	command += "\"";
	
	if (system(command.c_str()) != 0) {
		std::clog << "git config failed\n";
		std::exit(1);
	}

	// git config --add filter.git-crypt.clean "git-crypt clean /path/to/key"
	command = "git config --add filter.git-crypt.clean \"";
	command += git_crypt_path;
	command += " clean ";
	command += keyfile_path;
	command += "\"";
	
	if (system(command.c_str()) != 0) {
		std::clog << "git config failed\n";
		std::exit(1);
	}

	// git config --add diff.git-crypt.textconv "git-crypt diff /path/to/key"
	command = "git config --add diff.git-crypt.textconv \"";
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
	if (system("! git show-ref HEAD > /dev/null || git reset --hard HEAD") != 0) {
		std::clog << "git reset --hard failed\n";
		std::exit(1);
	}
}

void keygen (const char* keyfile)
{
	umask(0077); // make sure key file is protected
	std::ofstream	keyout(keyfile);
	if (!keyout) {
		perror(keyfile);
		std::exit(1);
	}
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
