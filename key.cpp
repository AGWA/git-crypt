/*
 * Copyright 2014 Andrew Ayer
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

#include "key.hpp"
#include "util.hpp"
#include "crypto.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fstream>
#include <istream>
#include <ostream>
#include <sstream>
#include <cstring>
#include <stdexcept>
#include <vector>

Key_file::Entry::Entry ()
{
	version = 0;
	explicit_memset(aes_key, 0, AES_KEY_LEN);
	explicit_memset(hmac_key, 0, HMAC_KEY_LEN);
}

void		Key_file::Entry::load (std::istream& in)
{
	while (true) {
		uint32_t	field_id;
		if (!read_be32(in, field_id)) {
			throw Malformed();
		}
		if (field_id == KEY_FIELD_END) {
			break;
		}
		uint32_t	field_len;
		if (!read_be32(in, field_len)) {
			throw Malformed();
		}

		if (field_id == KEY_FIELD_VERSION) {
			if (field_len != 4) {
				throw Malformed();
			}
			if (!read_be32(in, version)) {
				throw Malformed();
			}
		} else if (field_id == KEY_FIELD_AES_KEY) {
			if (field_len != AES_KEY_LEN) {
				throw Malformed();
			}
			in.read(reinterpret_cast<char*>(aes_key), AES_KEY_LEN);
			if (in.gcount() != AES_KEY_LEN) {
				throw Malformed();
			}
		} else if (field_id == KEY_FIELD_HMAC_KEY) {
			if (field_len != HMAC_KEY_LEN) {
				throw Malformed();
			}
			in.read(reinterpret_cast<char*>(hmac_key), HMAC_KEY_LEN);
			if (in.gcount() != HMAC_KEY_LEN) {
				throw Malformed();
			}
		} else if (field_id & 1) { // unknown critical field
			throw Incompatible();
		} else {
			// unknown non-critical field - safe to ignore
			if (field_len > MAX_FIELD_LEN) {
				throw Malformed();
			}
			in.ignore(field_len);
			if (in.gcount() != static_cast<std::streamsize>(field_len)) {
				throw Malformed();
			}
		}
	}
}

void		Key_file::Entry::load_legacy (uint32_t arg_version, std::istream& in)
{
	version = arg_version;

	// First comes the AES key
	in.read(reinterpret_cast<char*>(aes_key), AES_KEY_LEN);
	if (in.gcount() != AES_KEY_LEN) {
		throw Malformed();
	}

	// Then the HMAC key
	in.read(reinterpret_cast<char*>(hmac_key), HMAC_KEY_LEN);
	if (in.gcount() != HMAC_KEY_LEN) {
		throw Malformed();
	}

	if (in.peek() != -1) {
		// Trailing data is a good indication that we are not actually reading a
		// legacy key file.  (This is important to check since legacy key files
		// did not have any sort of file header.)
		throw Malformed();
	}
}

void		Key_file::Entry::store (std::ostream& out) const
{
	// Version
	write_be32(out, KEY_FIELD_VERSION);
	write_be32(out, 4);
	write_be32(out, version);

	// AES key
	write_be32(out, KEY_FIELD_AES_KEY);
	write_be32(out, AES_KEY_LEN);
	out.write(reinterpret_cast<const char*>(aes_key), AES_KEY_LEN);

	// HMAC key
	write_be32(out, KEY_FIELD_HMAC_KEY);
	write_be32(out, HMAC_KEY_LEN);
	out.write(reinterpret_cast<const char*>(hmac_key), HMAC_KEY_LEN);

	// End
	write_be32(out, KEY_FIELD_END);
}

void		Key_file::Entry::generate (uint32_t arg_version)
{
	version = arg_version;
	random_bytes(aes_key, AES_KEY_LEN);
	random_bytes(hmac_key, HMAC_KEY_LEN);
}

const Key_file::Entry*	Key_file::get_latest () const
{
	return is_filled() ? get(latest()) : 0;
}

const Key_file::Entry*	Key_file::get (uint32_t version) const
{
	Map::const_iterator	it(entries.find(version));
	return it != entries.end() ? &it->second : 0;
}

void		Key_file::add (const Entry& entry)
{
	entries[entry.version] = entry;
}


void		Key_file::load_legacy (std::istream& in)
{
	entries[0].load_legacy(0, in);
}

void		Key_file::load (std::istream& in)
{
	unsigned char	preamble[16];
	in.read(reinterpret_cast<char*>(preamble), 16);
	if (in.gcount() != 16) {
		throw Malformed();
	}
	if (std::memcmp(preamble, "\0GITCRYPTKEY", 12) != 0) {
		throw Malformed();
	}
	if (load_be32(preamble + 12) != FORMAT_VERSION) {
		throw Incompatible();
	}
	load_header(in);
	while (in.peek() != -1) {
		Entry		entry;
		entry.load(in);
		add(entry);
	}
}

void		Key_file::load_header (std::istream& in)
{
	while (true) {
		uint32_t	field_id;
		if (!read_be32(in, field_id)) {
			throw Malformed();
		}
		if (field_id == HEADER_FIELD_END) {
			break;
		}
		uint32_t	field_len;
		if (!read_be32(in, field_len)) {
			throw Malformed();
		}

		if (field_id == HEADER_FIELD_KEY_NAME) {
			if (field_len > KEY_NAME_MAX_LEN) {
				throw Malformed();
			}
			if (field_len == 0) {
				// special case field_len==0 to avoid possible undefined behavior
				// edge cases with an empty std::vector (particularly, &bytes[0]).
				key_name.clear();
			} else {
				std::vector<char>	bytes(field_len);
				in.read(&bytes[0], field_len);
				if (in.gcount() != static_cast<std::streamsize>(field_len)) {
					throw Malformed();
				}
				key_name.assign(&bytes[0], field_len);
			}
			if (!validate_key_name(key_name.c_str())) {
				key_name.clear();
				throw Malformed();
			}
		} else if (field_id & 1) { // unknown critical field
			throw Incompatible();
		} else {
			// unknown non-critical field - safe to ignore
			if (field_len > MAX_FIELD_LEN) {
				throw Malformed();
			}
			in.ignore(field_len);
			if (in.gcount() != static_cast<std::streamsize>(field_len)) {
				throw Malformed();
			}
		}
	}
}

void		Key_file::store (std::ostream& out) const
{
	out.write("\0GITCRYPTKEY", 12);
	write_be32(out, FORMAT_VERSION);
	if (!key_name.empty()) {
		write_be32(out, HEADER_FIELD_KEY_NAME);
		write_be32(out, key_name.size());
		out.write(key_name.data(), key_name.size());
	}
	write_be32(out, HEADER_FIELD_END);
	for (Map::const_iterator it(entries.begin()); it != entries.end(); ++it) {
		it->second.store(out);
	}
}

bool		Key_file::load_from_file (const char* key_file_name)
{
	std::ifstream	key_file_in(key_file_name, std::fstream::binary);
	if (!key_file_in) {
		return false;
	}
	load(key_file_in);
	return true;
}

bool		Key_file::store_to_file (const char* key_file_name) const
{
	create_protected_file(key_file_name);
	std::ofstream	key_file_out(key_file_name, std::fstream::binary);
	if (!key_file_out) {
		return false;
	}
	store(key_file_out);
	key_file_out.close();
	if (!key_file_out) {
		return false;
	}
	return true;
}

std::string	Key_file::store_to_string () const
{
	std::ostringstream	ss;
	store(ss);
	return ss.str();
}

void		Key_file::generate ()
{
	uint32_t	version(is_empty() ? 0 : latest() + 1);
	entries[version].generate(version);
}

uint32_t	Key_file::latest () const
{
	if (is_empty()) {
		throw std::invalid_argument("Key_file::latest");
	}
	return entries.begin()->first;
}

bool validate_key_name (const char* key_name, std::string* reason)
{
	if (!*key_name) {
		if (reason) { *reason = "Key name may not be empty"; }
		return false;
	}

	if (std::strcmp(key_name, "default") == 0) {
		if (reason) { *reason = "`default' is not a legal key name"; }
		return false;
	}
	// Need to be restrictive with key names because they're used as part of a Git filter name
	size_t		len = 0;
	while (char c = *key_name++) {
		if (!std::isalnum(c) && c != '-' && c != '_') {
			if (reason) { *reason = "Key names may contain only A-Z, a-z, 0-9, '-', and '_'"; }
			return false;
		}
		if (++len > KEY_NAME_MAX_LEN) {
			if (reason) { *reason = "Key name is too long"; }
			return false;
		}
	}
	return true;
}

