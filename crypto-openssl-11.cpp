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

#include <openssl/opensslconf.h>

#if defined(OPENSSL_API_COMPAT)

#include "crypto.hpp"
#include "key.hpp"
#include "util.hpp"
#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <sstream>
#include <cstring>

void init_crypto ()
{
	ERR_load_crypto_strings();
}

struct Aes_ecb_encryptor::Aes_impl {
	AES_KEY key;
};

Aes_ecb_encryptor::Aes_ecb_encryptor (const unsigned char* raw_key)
: impl(new Aes_impl)
{
	if (AES_set_encrypt_key(raw_key, KEY_LEN * 8, &(impl->key)) != 0) {
		throw Crypto_error("Aes_ctr_encryptor::Aes_ctr_encryptor", "AES_set_encrypt_key failed");
	}
}

Aes_ecb_encryptor::~Aes_ecb_encryptor ()
{
	// Note: Explicit destructor necessary because class contains an unique_ptr
	// which contains an incomplete type when the unique_ptr is declared.

	explicit_memset(&impl->key, '\0', sizeof(impl->key));
}

void Aes_ecb_encryptor::encrypt(const unsigned char* plain, unsigned char* cipher)
{
	AES_encrypt(plain, cipher, &(impl->key));
}

struct Hmac_sha1_state::Hmac_impl {
	HMAC_CTX *ctx;
};

Hmac_sha1_state::Hmac_sha1_state (const unsigned char* key, size_t key_len)
: impl(new Hmac_impl)
{

	impl->ctx = HMAC_CTX_new();
	HMAC_Init_ex(impl->ctx, key, key_len, EVP_sha1(), nullptr);
}

Hmac_sha1_state::~Hmac_sha1_state ()
{
	HMAC_CTX_free(impl->ctx);
}

void Hmac_sha1_state::add (const unsigned char* buffer, size_t buffer_len)
{
	HMAC_Update(impl->ctx, buffer, buffer_len);
}

void Hmac_sha1_state::get (unsigned char* digest)
{
	unsigned int len;
	HMAC_Final(impl->ctx, digest, &len);
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

#endif
