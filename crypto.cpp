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

#include "crypto.hpp"
#include "util.hpp"
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <sstream>
#include <cstring>
#include <cstdlib>

Aes_ctr_encryptor::Aes_ctr_encryptor (const unsigned char* raw_key, const unsigned char* arg_nonce)
{
	if (AES_set_encrypt_key(raw_key, KEY_LEN * 8, &key) != 0) {
		throw Crypto_error("Aes_ctr_encryptor::Aes_ctr_encryptor", "AES_set_encrypt_key failed");
	}

	std::memcpy(nonce, arg_nonce, NONCE_LEN);
	byte_counter = 0;
	std::memset(otp, '\0', sizeof(otp));
}

void Aes_ctr_encryptor::process (const unsigned char* in, unsigned char* out, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		if (byte_counter % BLOCK_LEN == 0) {
			unsigned char	ctr[BLOCK_LEN];

			// First 12 bytes of CTR: nonce
			std::memcpy(ctr, nonce, NONCE_LEN);

			// Last 4 bytes of CTR: block number (sequentially increasing with each block) (big endian)
			store_be32(ctr + NONCE_LEN, byte_counter / BLOCK_LEN);

			// Generate a new OTP
			AES_encrypt(ctr, otp, &key);
		}

		// encrypt one byte
		out[i] = in[i] ^ otp[byte_counter++ % BLOCK_LEN];

		if (byte_counter == 0) {
			throw Crypto_error("Aes_ctr_encryptor::process", "Too much data to encrypt securely");
		}
	}
}

Hmac_sha1_state::Hmac_sha1_state (const unsigned char* key, size_t key_len)
{
	HMAC_Init(&ctx, key, key_len, EVP_sha1());
}

Hmac_sha1_state::~Hmac_sha1_state ()
{
	HMAC_cleanup(&ctx);
}

void Hmac_sha1_state::add (const unsigned char* buffer, size_t buffer_len)
{
	HMAC_Update(&ctx, buffer, buffer_len);
}

void Hmac_sha1_state::get (unsigned char* digest)
{
	unsigned int len;
	HMAC_Final(&ctx, digest, &len);
}


// Encrypt/decrypt an entire input stream, writing to the given output stream
void Aes_ctr_encryptor::process_stream (std::istream& in, std::ostream& out, const unsigned char* key, const unsigned char* nonce)
{
	Aes_ctr_encryptor	aes(key, nonce);

	unsigned char		buffer[1024];
	while (in) {
		in.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
		aes.process(buffer, buffer, in.gcount());
		out.write(reinterpret_cast<char*>(buffer), in.gcount());
	}
}

void random_bytes (unsigned char* buffer, size_t len)
{
	if (RAND_bytes(buffer, len) != 1) {
		std::ostringstream	message;
		while (unsigned long code = ERR_get_error()) {
			char		error_string[120];
			ERR_error_string_n(code, error_string, sizeof(error_string));
			message << "OpenSSL Error: " << error_string << "; ";
		}
		throw Crypto_error("random_bytes", message.str());
	}
}

