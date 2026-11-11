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
#include "key.hpp"
#include "util.hpp"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <sstream>
#include <cstring>

// This backend uses only OpenSSL's EVP interfaces, which are not deprecated in
// OpenSSL 3 and later.  EVP_PKEY_new_raw_private_key() requires OpenSSL 1.1.1.
#if OPENSSL_VERSION_NUMBER < 0x10101000L
#error "git-crypt requires OpenSSL 1.1.1 or newer"
#endif

// Returns (and clears) OpenSSL's queued error messages, for use in exception messages.
static std::string openssl_errors ()
{
	std::ostringstream	message;
	while (unsigned long code = ERR_get_error()) {
		char		error_string[120];
		ERR_error_string_n(code, error_string, sizeof(error_string));
		message << "OpenSSL Error: " << error_string << "; ";
	}
	return message.str();
}

void init_crypto ()
{
	// OpenSSL 1.1.0 and later initialize themselves; this just makes sure the
	// error strings used by openssl_errors() are loaded.
	OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
}

struct Aes_ecb_encryptor::Aes_impl {
	EVP_CIPHER_CTX*		ctx;
};

Aes_ecb_encryptor::Aes_ecb_encryptor (const unsigned char* raw_key)
: impl(new Aes_impl)
{
	// A single AES-256 block encryption is AES-256 in ECB mode, without padding,
	// applied to one block at a time.
	impl->ctx = EVP_CIPHER_CTX_new();
	if (!impl->ctx) {
		throw Crypto_error("Aes_ecb_encryptor::Aes_ecb_encryptor", "EVP_CIPHER_CTX_new failed: " + openssl_errors());
	}
	if (EVP_EncryptInit_ex(impl->ctx, EVP_aes_256_ecb(), nullptr, raw_key, nullptr) != 1 ||
	    EVP_CIPHER_CTX_key_length(impl->ctx) != KEY_LEN ||
	    EVP_CIPHER_CTX_set_padding(impl->ctx, 0) != 1) {
		const std::string	errors(openssl_errors());
		EVP_CIPHER_CTX_free(impl->ctx);
		throw Crypto_error("Aes_ecb_encryptor::Aes_ecb_encryptor", "EVP_EncryptInit_ex failed: " + errors);
	}
}

Aes_ecb_encryptor::~Aes_ecb_encryptor ()
{
	// Note: Explicit destructor necessary because class contains an unique_ptr
	// which contains an incomplete type when the unique_ptr is declared.

	// Also cleanses the expanded key held by the context.
	EVP_CIPHER_CTX_free(impl->ctx);
}

void Aes_ecb_encryptor::encrypt(const unsigned char* plain, unsigned char* cipher)
{
	int		out_len = 0;
	if (EVP_EncryptUpdate(impl->ctx, cipher, &out_len, plain, BLOCK_LEN) != 1 || out_len != BLOCK_LEN) {
		throw Crypto_error("Aes_ecb_encryptor::encrypt", "EVP_EncryptUpdate failed: " + openssl_errors());
	}
}

struct Hmac_sha1_state::Hmac_impl {
	EVP_PKEY*		key;
	EVP_MD_CTX*		ctx;
};

Hmac_sha1_state::Hmac_sha1_state (const unsigned char* key, size_t key_len)
: impl(new Hmac_impl)
{
	impl->key = EVP_PKEY_new_raw_private_key(EVP_PKEY_HMAC, nullptr, key, key_len);
	impl->ctx = EVP_MD_CTX_new();
	if (!impl->key || !impl->ctx || EVP_DigestSignInit(impl->ctx, nullptr, EVP_sha1(), nullptr, impl->key) != 1) {
		const std::string	errors(openssl_errors());
		EVP_MD_CTX_free(impl->ctx);
		EVP_PKEY_free(impl->key);
		throw Crypto_error("Hmac_sha1_state::Hmac_sha1_state", "HMAC-SHA1 initialization failed: " + errors);
	}
}

Hmac_sha1_state::~Hmac_sha1_state ()
{
	EVP_MD_CTX_free(impl->ctx);
	EVP_PKEY_free(impl->key);
}

void Hmac_sha1_state::add (const unsigned char* buffer, size_t buffer_len)
{
	if (EVP_DigestSignUpdate(impl->ctx, buffer, buffer_len) != 1) {
		throw Crypto_error("Hmac_sha1_state::add", "EVP_DigestSignUpdate failed: " + openssl_errors());
	}
}

void Hmac_sha1_state::get (unsigned char* digest)
{
	size_t		len = LEN;
	if (EVP_DigestSignFinal(impl->ctx, digest, &len) != 1 || len != LEN) {
		throw Crypto_error("Hmac_sha1_state::get", "EVP_DigestSignFinal failed: " + openssl_errors());
	}
}


void random_bytes (unsigned char* buffer, size_t len)
{
	if (RAND_bytes(buffer, len) != 1) {
		throw Crypto_error("random_bytes", openssl_errors());
	}
}
