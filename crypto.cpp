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
#include <endian.h>

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

void aes_ctr_state::process_block (const AES_KEY* key, const uint8_t* in, uint8_t* out, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		if (byte_counter % 16 == 0) {
			// Generate a new OTP
			// CTR value:
			//  first 12 bytes - nonce
			//  last   4 bytes - block number (sequentially increasing with each block)
			uint8_t		ctr[16];
			uint32_t	blockno = htole32(byte_counter / 16);
			memcpy(ctr, nonce, 12);
			memcpy(ctr + 12, &blockno, 4);
			AES_encrypt(ctr, otp, key);
		}

		// encrypt one byte
		out[i] = in[i] ^ otp[byte_counter++ % 16];
	}
}

// Compute HMAC-SHA1-96 (i.e. first 96 bits of HMAC-SHA1) for the given buffer with the given key
void hmac_sha1_96 (uint8_t* out, const uint8_t* buffer, size_t buffer_len, const uint8_t* key, size_t key_len)
{
	uint8_t	full_digest[20];
	HMAC(EVP_sha1(), key, key_len, buffer, buffer_len, full_digest, NULL);
	memcpy(out, full_digest, 12); // Truncate to first 96 bits
}

// Encrypt/decrypt an entire input stream, writing to the given output stream
void process_stream (std::istream& in, std::ostream& out, const AES_KEY* enc_key, const uint8_t* nonce)
{
	aes_ctr_state	state(nonce, 12);

	uint8_t		buffer[1024];
	while (in) {
		in.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
		state.process_block(enc_key, buffer, buffer, in.gcount());
		out.write(reinterpret_cast<char*>(buffer), in.gcount());
	}
}
