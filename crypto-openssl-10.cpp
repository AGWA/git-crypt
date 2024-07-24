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

#if !defined(OPENSSL_API_COMPAT)

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
        EVP_CIPHER_CTX *ctx;
};

Aes_ecb_encryptor::Aes_ecb_encryptor (const unsigned char* raw_key)
: impl(new Aes_impl)
{
        impl->ctx = EVP_CIPHER_CTX_new();
        if (!EVP_EncryptInit_ex(impl->ctx, EVP_aes_256_ecb(), nullptr, raw_key, nullptr)) {
                throw Crypto_error("Aes_ecb_encryptor::Aes_ecb_encryptor", "EVP_EncryptInit_ex failed");
        }
}

Aes_ecb_encryptor::~Aes_ecb_encryptor ()
{
        // Note: Explicit destructor necessary because class contains a unique_ptr
        // which contains an incomplete type when the unique_ptr is declared.

        EVP_CIPHER_CTX_free(impl->ctx);
}

void Aes_ecb_encryptor::encrypt(const unsigned char* plain, unsigned char* cipher)
{
        int outlen;
        if (!EVP_EncryptUpdate(impl->ctx, cipher, &outlen, plain, AES_BLOCK_SIZE) || outlen != AES_BLOCK_SIZE) {
                throw Crypto_error("Aes_ecb_encryptor::encrypt", "EVP_EncryptUpdate failed");
        }
}

struct Hmac_sha1_state::Hmac_impl {
        EVP_MD_CTX *ctx;
        const EVP_MD *md;
};

Hmac_sha1_state::Hmac_sha1_state (const unsigned char* key, size_t key_len)
: impl(new Hmac_impl)
{
        impl->ctx = EVP_MD_CTX_new();
        impl->md = EVP_sha1();
        if (!EVP_DigestInit_ex(impl->ctx, impl->md, nullptr) ||
            !EVP_DigestSignInit(impl->ctx, nullptr, impl->md, nullptr, nullptr) ||
            !EVP_DigestSignUpdate(impl->ctx, key, key_len)) {
                throw Crypto_error("Hmac_sha1_state::Hmac_sha1_state", "EVP_DigestSignInit or EVP_DigestSignUpdate failed");
        }
}

Hmac_sha1_state::~Hmac_sha1_state ()
{
        // Note: Explicit destructor necessary because class contains a unique_ptr
        // which contains an incomplete type when the unique_ptr is declared.

        EVP_MD_CTX_free(impl->ctx);
}

void Hmac_sha1_state::add (const unsigned char* buffer, size_t buffer_len)
{
        EVP_DigestSignUpdate(impl->ctx, buffer, buffer_len);
}

void Hmac_sha1_state::get (unsigned char* digest)
{
        size_t len;
        EVP_DigestSignFinal(impl->ctx, digest, &len);
}

void random_bytes (unsigned char* buffer, size_t len)
{
        if (RAND_bytes(buffer, len) != 1) {
                std::ostringstream      message;
                while (unsigned long code = ERR_get_error()) {
                        char            error_string[120];
                        ERR_error_string_n(code, error_string, sizeof(error_string));
                        message << "OpenSSL Error: " << error_string << "; ";
                }
                throw Crypto_error("random_bytes", message.str());
        }
}

#endif
