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

#ifndef GIT_CRYPT_GPG_HPP
#define GIT_CRYPT_GPG_HPP

#include <string>
#include <vector>
#include <cstddef>

struct Gpg_error {
	std::string	message;

	explicit Gpg_error (std::string m) : message(m) { }
};

std::string			gpg_get_uid (const std::string& fingerprint);
std::vector<std::string>	gpg_lookup_key (const std::string& query);
std::vector<std::string>	gpg_list_secret_keys ();
void				gpg_encrypt_to_file (const std::string& filename, const std::string& recipient_fingerprint, bool key_is_trusted, const char* p, size_t len);
void				gpg_decrypt_from_file (const std::string& filename, std::ostream&);

#endif
