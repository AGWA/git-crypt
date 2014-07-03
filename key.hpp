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

#ifndef GIT_CRYPT_KEY_HPP
#define GIT_CRYPT_KEY_HPP

#include <map>
#include <functional>
#include <stdint.h>
#include <iosfwd>
#include <string>

enum {
	HMAC_KEY_LEN = 64,
	AES_KEY_LEN = 32
};

struct Key_file {
public:
	struct Entry {
		uint32_t		version;
		unsigned char		aes_key[AES_KEY_LEN];
		unsigned char		hmac_key[HMAC_KEY_LEN];

		Entry ();

		void			load (std::istream&);
		void			load_legacy (uint32_t version, std::istream&);
		void			store (std::ostream&) const;
		void			generate (uint32_t version);
	};

	struct Malformed { }; // exception class
	struct Incompatible { }; // exception class

	const Entry*			get_latest () const;

	const Entry*			get (uint32_t version) const;
	void				add (const Entry&);

	void				load_legacy (std::istream&);
	void				load (std::istream&);
	void				store (std::ostream&) const;

	bool				load_from_file (const char* filename);
	bool				store_to_file (const char* filename) const;

	std::string			store_to_string () const;

	void				generate ();

	bool				is_empty () const { return entries.empty(); }
	bool				is_filled () const { return !is_empty(); }

	uint32_t			latest () const;

	void				set_key_name (const char* k) { key_name = k ? k : ""; }
	const char*			get_key_name () const { return key_name.empty() ? 0 : key_name.c_str(); }
private:
	typedef std::map<uint32_t, Entry, std::greater<uint32_t> > Map;
	enum { FORMAT_VERSION = 2 };

	Map				entries;
	std::string			key_name;

	void				load_header (std::istream&);

	enum {
		HEADER_FIELD_END	= 0,
		HEADER_FIELD_KEY_NAME	= 1
	};
	enum {
		KEY_FIELD_END		= 0,
		KEY_FIELD_VERSION	= 1,
		KEY_FIELD_AES_KEY	= 3,
		KEY_FIELD_HMAC_KEY	= 5
	};
	enum {
		MAX_FIELD_LEN		= 1<<20
	};
};

enum {
	KEY_NAME_MAX_LEN = 128
};

bool validate_key_name (const char* key_name, std::string* reason =0);

#endif
