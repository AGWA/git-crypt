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

#ifndef GIT_CRYPT_CRYPTO_HPP
#define GIT_CRYPT_CRYPTO_HPP

#include "key.hpp"
#include <stdint.h>
#include <stddef.h>
#include <iosfwd>
#include <string>
#include <memory>

void init_crypto ();

struct Crypto_error {
	std::string	where;
	std::string	message;

	Crypto_error (const std::string& w, const std::string& m) : where(w), message(m) { }
};

class Aes_ecb_encryptor {
public:
	enum {
		KEY_LEN		= AES_KEY_LEN,
		BLOCK_LEN	= 16
	};

private:
	struct Aes_impl;

	std::unique_ptr<Aes_impl>	impl;

public:
	Aes_ecb_encryptor (const unsigned char* key);
	~Aes_ecb_encryptor ();
	void encrypt (const unsigned char* plain, unsigned char* cipher);
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
	Aes_ecb_encryptor	ecb;
	unsigned char		ctr_value[BLOCK_LEN];	// Current CTR value (used as input to AES to derive pad)
	unsigned char		pad[BLOCK_LEN];		// Current encryption pad (output of AES)
	uint32_t		byte_counter;		// How many bytes processed so far?

public:
	Aes_ctr_encryptor (const unsigned char* key, const unsigned char* nonce);
	~Aes_ctr_encryptor ();

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
	struct Hmac_impl;

	std::unique_ptr<Hmac_impl>	impl;

public:
	Hmac_sha1_state (const unsigned char* key, size_t key_len);
	~Hmac_sha1_state ();

	void add (const unsigned char* buffer, size_t buffer_len);
	void get (unsigned char*);
};

void random_bytes (unsigned char*, size_t);

#endif
