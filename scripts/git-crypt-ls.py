#!/usr/bin/env python3
# -*- coding:utf-8 -*-
#
#                   GNU GENERAL PUBLIC LICENSE
#                       Version 2, June 1991
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
# 
# 2019.11.18 works
#
import sys
import os
import re
import getopt
import glob

def find_files(path):
    root_prefix = os.path.abspath(path)

    all_files = []
    for prefix, dirs, files in os.walk(path):
        for fn in files:
            full_name = os.path.join(prefix, fn)
            full_name = os.path.abspath(full_name)
            full_name = full_name.replace(root_prefix, '.')
            if full_name.startswith('.\\.git\\'):
                continue

            all_files.append(full_name)

    return all_files

if __name__ == '__main__':
    all_files = find_files('.')

    print('Info: find', len(all_files), 'files')
    print()

    l = []
    for fn in all_files:
        f = open(fn, 'rb')
        d = f.read(10)
        f.close()

        if d.startswith(b'\x00GITCRYPT\x00'):
            l.append(fn)

    if len(l) > 0:
        for fn in l:
            print('[encrypted]', fn)
        print()
        print('Info: encrypted', len(l))
        print('Info: plaintext', len(all_files) - len(l))
    else:
        print('Info: no files encrypted')


