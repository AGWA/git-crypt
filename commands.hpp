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

#ifndef GIT_CRYPT_COMMANDS_HPP
#define GIT_CRYPT_COMMANDS_HPP

#include <string>
#include <iosfwd>

struct Error {
	std::string	message;

	explicit Error (std::string m) : message(m) { }
};

// Plumbing commands:
int clean (int argc, const char** argv);
int smudge (int argc, const char** argv);
int diff (int argc, const char** argv);
// Public commands:
int init (int argc, const char** argv);
int unlock (int argc, const char** argv);
int lock (int argc, const char** argv);
int add_gpg_user (int argc, const char** argv);
int rm_gpg_user (int argc, const char** argv);
int ls_gpg_users (int argc, const char** argv);
int export_key (int argc, const char** argv);
int keygen (int argc, const char** argv);
int migrate_key (int argc, const char** argv);
int refresh (int argc, const char** argv);
int status (int argc, const char** argv);

// Help messages:
void help_init (std::ostream&);
void help_unlock (std::ostream&);
void help_lock (std::ostream&);
void help_add_gpg_user (std::ostream&);
void help_rm_gpg_user (std::ostream&);
void help_ls_gpg_users (std::ostream&);
void help_export_key (std::ostream&);
void help_keygen (std::ostream&);
void help_migrate_key (std::ostream&);
void help_refresh (std::ostream&);
void help_status (std::ostream&);

// other
std::string get_git_config (const std::string& name);

#endif
