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
#include <cstring>

Aes_ctr_encryptor::Aes_ctr_encryptor (const unsigned char* raw_key, const unsigned char* nonce)
: ecb(raw_key)
{
	// Set first 12 bytes of the CTR value to the nonce.
	// This stays the same for the entirety of this object's lifetime.
	std::memcpy(ctr_value, nonce, NONCE_LEN);
	byte_counter = 0;
}

Aes_ctr_encryptor::~Aes_ctr_encryptor ()
{
	explicit_memset(pad, '\0', BLOCK_LEN);
}

void Aes_ctr_encryptor::process (const unsigned char* in, unsigned char* out, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		if (byte_counter % BLOCK_LEN == 0) {
			// Set last 4 bytes of CTR to the (big-endian) block number (sequentially increasing with each block)
			store_be32(ctr_value + NONCE_LEN, byte_counter / BLOCK_LEN);

			// Generate a new pad
			ecb.encrypt(ctr_value, pad);
		}

		// encrypt one byte
		out[i] = in[i] ^ pad[byte_counter++ % BLOCK_LEN];

		if (byte_counter == 0) {
			throw Crypto_error("Aes_ctr_encryptor::process", "Too much data to encrypt securely");
		}
	}
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

