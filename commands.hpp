/*
 * Copyright 2012 Andrew Ayer
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
 */

#ifndef _COMMANDS_H
#define _COMMANDS_H


void clean (const char* keyfile);
void smudge (const char* keyfile);
void diff (const char* keyfile, const char* filename);
void init (const char* argv0, const char* keyfile);
void keygen (const char* keyfile);

#endif

