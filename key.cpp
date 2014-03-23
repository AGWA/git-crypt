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
#include <fstream>
#include <istream>
#include <ostream>
#include <cstring>
#include <stdexcept>

void		Key_file::Entry::load (std::istream& in)
{
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
}

void		Key_file::Entry::store (std::ostream& out) const
{
	out.write(reinterpret_cast<const char*>(aes_key), AES_KEY_LEN);
	out.write(reinterpret_cast<const char*>(hmac_key), HMAC_KEY_LEN);
}

void		Key_file::Entry::generate ()
{
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

void		Key_file::add (uint32_t version, const Entry& entry)
{
	entries[version] = entry;
}


void		Key_file::load_legacy (std::istream& in)
{
	entries[0].load(in);
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
	while (in.peek() != -1) {
		uint32_t	version;
		if (!read_be32(in, version)) {
			throw Malformed();
		}
		entries[version].load(in);
	}
}

void		Key_file::store (std::ostream& out) const
{
	out.write("\0GITCRYPTKEY", 12);
	write_be32(out, FORMAT_VERSION);
	for (Map::const_iterator it(entries.begin()); it != entries.end(); ++it) {
		write_be32(out, it->first);
		it->second.store(out);
	}
}

bool		Key_file::load (const char* key_file_name)
{
	std::ifstream	key_file_in(key_file_name, std::fstream::binary);
	if (!key_file_in) {
		return false;
	}
	load(key_file_in);
	return true;
}

bool		Key_file::store (const char* key_file_name) const
{
	mode_t		old_umask = umask(0077); // make sure key file is protected
	std::ofstream	key_file_out(key_file_name, std::fstream::binary);
	umask(old_umask);
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

void		Key_file::generate ()
{
	entries[is_empty() ? 0 : latest() + 1].generate();
}

uint32_t	Key_file::latest () const
{
	if (is_empty()) {
		throw std::invalid_argument("Key_file::latest");
	}
	return entries.begin()->first;
}

