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

#ifndef _CRYPTO_H
#define _CRYPTO_H

#include "key.hpp"
#include <openssl/aes.h>
#include <openssl/hmac.h>
#include <stdint.h>
#include <stddef.h>
#include <iosfwd>
#include <string>

struct Crypto_error {
	std::string	where;
	std::string	message;

	Crypto_error (const std::string& w, const std::string& m) : where(w), message(m) { }
};

class Aes_ctr_encryptor {
public:
	enum {
		NONCE_LEN	= 12,
		KEY_LEN		= AES_KEY_LEN,
		BLOCK_LEN	= 16,
		MAX_CRYPT_BYTES	= (1ULL<<32)*16 // Don't encrypt more than this or the CTR value will repeat itself
	};

private:
	AES_KEY		key;
	char		nonce[NONCE_LEN];// First 96 bits of counter
	uint32_t	byte_counter;	// How many bytes processed so far?
	unsigned char	otp[BLOCK_LEN];	// The current OTP that's in use

public:
	Aes_ctr_encryptor (const unsigned char* key, const unsigned char* nonce);

	void process (const unsigned char* in, unsigned char* out, size_t len);

	// Encrypt/decrypt an entire input stream, writing to the given output stream
	static void process_stream (std::istream& in, std::ostream& out, const unsigned char* key, const unsigned char* nonce);
};

typedef Aes_ctr_encryptor Aes_ctr_decryptor;

class Hmac_sha1_state {
public:
	enum {
		LEN	= 20,
		KEY_LEN	= HMAC_KEY_LEN
	};

private:
	HMAC_CTX	ctx;

	// disallow copy/assignment:
	Hmac_sha1_state (const Hmac_sha1_state&) { }
	Hmac_sha1_state& operator= (const Hmac_sha1_state&) { return *this; }

public:
	Hmac_sha1_state (const unsigned char* key, size_t key_len);
	~Hmac_sha1_state ();

	void add (const unsigned char* buffer, size_t buffer_len);
	void get (unsigned char*);
};

void random_bytes (unsigned char*, size_t);

#endif
