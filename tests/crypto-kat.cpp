/*
 * Known-answer tests for git-crypt's crypto primitives (Aes_ecb_encryptor and
 * Hmac_sha1_state), using published test vectors.  Built and run by 'make test'.
 *
 * See COPYING file for license information.
 */

#include "../crypto.hpp"
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

const char*	argv0 = "crypto-kat"; // referenced by util.cpp

static int	failures = 0;

static std::vector<unsigned char> from_hex (const std::string& hex)
{
	std::vector<unsigned char>	bytes;
	for (std::string::size_type i = 0; i + 1 < hex.size(); i += 2) {
		bytes.push_back(static_cast<unsigned char>(std::stoul(hex.substr(i, 2), nullptr, 16)));
	}
	return bytes;
}

static std::string to_hex (const unsigned char* p, size_t len)
{
	static const char	digits[] = "0123456789abcdef";
	std::string		hex;
	for (size_t i = 0; i < len; ++i) {
		hex.push_back(digits[p[i] >> 4]);
		hex.push_back(digits[p[i] & 0xf]);
	}
	return hex;
}

static void check (const std::string& name, const std::string& expected_hex, const unsigned char* actual, size_t len)
{
	const std::string	actual_hex(to_hex(actual, len));
	if (actual_hex == expected_hex) {
		std::cout << "ok      " << name << std::endl;
	} else {
		std::cout << "FAILED  " << name << ": expected " << expected_hex << ", got " << actual_hex << std::endl;
		++failures;
	}
}

// AES-256 encryption of single blocks, reusing one Aes_ecb_encryptor for several blocks
static void aes_256_kat (const std::string& name, const std::string& key_hex, const char* const* plain_cipher_hex_pairs)
{
	const std::vector<unsigned char>	key(from_hex(key_hex));
	Aes_ecb_encryptor			aes(&key[0]);
	for (int i = 0; plain_cipher_hex_pairs[i]; i += 2) {
		const std::vector<unsigned char>	plain(from_hex(plain_cipher_hex_pairs[i]));
		unsigned char				cipher[Aes_ecb_encryptor::BLOCK_LEN];
		aes.encrypt(&plain[0], cipher);
		check(name + " block " + std::to_string(i / 2 + 1), plain_cipher_hex_pairs[i + 1], cipher, sizeof(cipher));
	}
}

// HMAC-SHA1, both in one add() call and byte by byte
static void hmac_sha1_kat (const std::string& name, const std::vector<unsigned char>& key, const std::vector<unsigned char>& data, const std::string& digest_hex)
{
	unsigned char		digest[Hmac_sha1_state::LEN];
	{
		Hmac_sha1_state	hmac(&key[0], key.size());
		hmac.add(&data[0], data.size());
		hmac.get(digest);
		check(name, digest_hex, digest, sizeof(digest));
	}
	{
		Hmac_sha1_state	hmac(&key[0], key.size());
		for (size_t i = 0; i < data.size(); ++i) {
			hmac.add(&data[i], 1);
		}
		hmac.get(digest);
		check(name + " (incremental)", digest_hex, digest, sizeof(digest));
	}
}

static std::vector<unsigned char> bytes (const std::string& s)
{
	return std::vector<unsigned char>(s.begin(), s.end());
}

static std::vector<unsigned char> repeat (unsigned char c, size_t n)
{
	return std::vector<unsigned char>(n, c);
}

int main ()
{
	init_crypto();

	// FIPS-197 Appendix C.3
	static const char* const	fips197_c3[] = {
		"00112233445566778899aabbccddeeff", "8ea2b7ca516745bfeafc49904b496089",
		nullptr
	};
	aes_256_kat("AES-256 FIPS-197 C.3", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", fips197_c3);

	// NIST SP 800-38A F.1.5 (ECB-AES256.Encrypt)
	static const char* const	sp800_38a_f15[] = {
		"6bc1bee22e409f96e93d7e117393172a", "f3eed1bdb5d2a03c064b5a7e3db181f8",
		"ae2d8a571e03ac9c9eb76fac45af8e51", "591ccb10d410ed26dc5ba74a31362870",
		"30c81c46a35ce411e5fbc1191a0a52ef", "b6ed21b99ca6f4f9f153e7b1beafed1d",
		"f69f2445df4f9b17ad2b417be66c3710", "23304b7a39f9f3ff067d8d8f9e24ecc7",
		nullptr
	};
	aes_256_kat("AES-256 SP 800-38A F.1.5", "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", sp800_38a_f15);

	// RFC 2202 section 3 (HMAC-SHA-1)
	hmac_sha1_kat("HMAC-SHA1 RFC 2202 #1", repeat(0x0b, 20), bytes("Hi There"), "b617318655057264e28bc0b6fb378c8ef146be00");
	hmac_sha1_kat("HMAC-SHA1 RFC 2202 #2", bytes("Jefe"), bytes("what do ya want for nothing?"), "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
	hmac_sha1_kat("HMAC-SHA1 RFC 2202 #3", repeat(0xaa, 20), repeat(0xdd, 50), "125d7342b9ac11cd91a39af48aa17b4f63f175d3");
	hmac_sha1_kat("HMAC-SHA1 RFC 2202 #4", from_hex("0102030405060708090a0b0c0d0e0f10111213141516171819"), repeat(0xcd, 50), "4c9007f4026250c6bc8414f9bf50c86c2d7235da");
	hmac_sha1_kat("HMAC-SHA1 RFC 2202 #5", repeat(0x0c, 20), bytes("Test With Truncation"), "4c1a03424b55e07fe7f27be1d58bb9324a9a5a04");
	hmac_sha1_kat("HMAC-SHA1 RFC 2202 #6", repeat(0xaa, 80), bytes("Test Using Larger Than Block-Size Key - Hash Key First"), "aa4ae5e15272d00e95705637ce8a3b55ed402112");
	hmac_sha1_kat("HMAC-SHA1 RFC 2202 #7", repeat(0xaa, 80), bytes("Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data"), "e8e99d0f45237d786d6bbaa7965c7808bbff1a91");

	std::cout << std::endl << (failures ? "crypto known-answer tests FAILED" : "crypto known-answer tests passed") << std::endl;
	return failures ? 1 : 0;
}
