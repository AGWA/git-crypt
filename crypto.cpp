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

#define _BSD_SOURCE
#include "crypto.hpp"
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <arpa/inet.h>

void load_keys (const char* filepath, keys_t* keys)
{
	std::ifstream	file(filepath);
	if (!file) {
		perror(filepath);
		std::exit(1);
	}
	char	buffer[AES_KEY_BITS/8 + HMAC_KEY_LEN];
	file.read(buffer, sizeof(buffer));
	if (file.gcount() != sizeof(buffer)) {
		std::clog << filepath << ": Premature end of key file\n";
		std::exit(1);
	}

	// First comes the AES encryption key
	if (AES_set_encrypt_key(reinterpret_cast<uint8_t*>(buffer), AES_KEY_BITS, &keys->enc) != 0) {
		std::clog << filepath << ": Failed to initialize AES encryption key\n";
		std::exit(1);
	}

	// Then it's the HMAC key
	memcpy(keys->hmac, buffer + AES_KEY_BITS/8, HMAC_KEY_LEN);
}


aes_ctr_state::aes_ctr_state (const uint8_t* arg_nonce, size_t arg_nonce_len)
{
	memset(nonce, '\0', sizeof(nonce));
	memcpy(nonce, arg_nonce, std::min(arg_nonce_len, sizeof(nonce)));
	byte_counter = 0;
	memset(otp, '\0', sizeof(otp));
}

void aes_ctr_state::process (const AES_KEY* key, const uint8_t* in, uint8_t* out, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		if (byte_counter % 16 == 0) {
			// Generate a new OTP
			// CTR value:
			//  first 12 bytes - nonce
			//  last   4 bytes - block number (sequentially increasing with each block)
			uint8_t		ctr[16];
			uint32_t	blockno = htonl(byte_counter / 16);
			memcpy(ctr, nonce, 12);
			memcpy(ctr + 12, &blockno, 4);
			AES_encrypt(ctr, otp, key);
		}

		// encrypt one byte
		out[i] = in[i] ^ otp[byte_counter++ % 16];
	}
}

hmac_sha1_state::hmac_sha1_state (const uint8_t* key, size_t key_len)
{
	HMAC_Init(&ctx, key, key_len, EVP_sha1());
}

hmac_sha1_state::~hmac_sha1_state ()
{
	HMAC_cleanup(&ctx);
}

void hmac_sha1_state::add (const uint8_t* buffer, size_t buffer_len)
{
	HMAC_Update(&ctx, buffer, buffer_len);
}

void hmac_sha1_state::get (uint8_t* digest)
{
	unsigned int len;
	HMAC_Final(&ctx, digest, &len);
}


// Encrypt/decrypt an entire input stream, writing to the given output stream
void process_stream (std::istream& in, std::ostream& out, const AES_KEY* enc_key, const uint8_t* nonce)
{
	aes_ctr_state	state(nonce, 12);

	uint8_t		buffer[1024];
	while (in) {
		in.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
		state.process(enc_key, buffer, buffer, in.gcount());
		out.write(reinterpret_cast<char*>(buffer), in.gcount());
	}
}
