#ifndef _CRYPTO_H
#define _CRYPTO_H

#include <openssl/aes.h>
#include <openssl/hmac.h>
#include <stdint.h>
#include <stddef.h>
#include <iosfwd>

enum {
	SHA1_LEN = 20,
	NONCE_LEN = 12,
	HMAC_KEY_LEN = 64,
	AES_KEY_BITS = 256,
	MAX_CRYPT_BYTES = (1ULL<<32)*16	// Don't encrypt more than this or the CTR value will repeat itself
};

struct keys_t {
	AES_KEY		enc;
	uint8_t		hmac[HMAC_KEY_LEN];
};
void load_keys (const char* filepath, keys_t* keys);

class aes_ctr_state {
	char		nonce[NONCE_LEN];// First 96 bits of counter
	uint32_t	byte_counter;	// How many bytes processed so far?
	uint8_t		otp[16];	// The current OTP that's in use

public:
	aes_ctr_state (const uint8_t* arg_nonce, size_t arg_nonce_len);

	void process (const AES_KEY* key, const uint8_t* in, uint8_t* out, size_t len);
};

class hmac_sha1_state {
	HMAC_CTX	ctx;

	// disallow copy/assignment:
	hmac_sha1_state (const hmac_sha1_state&) { }
	hmac_sha1_state& operator= (const hmac_sha1_state&) { return *this; }
public:
	hmac_sha1_state (const uint8_t* key, size_t key_len);
	~hmac_sha1_state ();

	void add (const uint8_t* buffer, size_t buffer_len);
	void get (uint8_t*);
};

// Encrypt/decrypt an entire input stream, writing to the given output stream
void process_stream (std::istream& in, std::ostream& out, const AES_KEY* enc_key, const uint8_t* nonce);


#endif
