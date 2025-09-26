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
#
# 2019.11.11 search git-crypt.key 
# 2019.11.18 decrypt algorithm done
# 2020.03.06 search all files and decrypt
#
# how it works:
#   1. git-crypt generate two keys: 256bits AES, 512bits HMAC
#   2. git-crypt call gpg to encrypt those keys.
#   3. file_data -> HMAC(hmackey, sha1) -> keep first 12 bytes -> nonce
#   4. file_data -> AES_CTR(aeskey, nonce) -> encrypted_data
#
import sys
import os
import re
import getopt
import struct
import glob
import base64

#
# git-crypt choose ECB mode with hand-written CTR
# and the python library `pycrypto` did not have AES_CTR
# I rewrite this CTR from git-crypt.
#
from Crypto.Cipher import AES
from Crypto.Hash import HMAC
from Crypto.Hash import SHA1

KEY_FIELD_AES_KEY   = 3
KEY_FIELD_HMAC_KEY  = 5
HMAC_KEY_LEN        = 64    # 512bits
AES_KEY_LEN         = 32    # 256bits
AES_BLOCK_LEN       = 16    # 128bits
HMAC_CHECK_LEN      = 12    # Using as signature

def load_keyfile(begin_path='.', git_crypt_keyfile=None):
    # find keyfile
    if git_crypt_keyfile is None:
        repo_path = os.path.abspath(begin_path)
        while True:
            gitdir = os.path.join(repo_path, '.git')
            if os.path.isdir(gitdir):
                break
            repo_path = os.path.split(repo_path)[0]
            if repo_path == '' or repo_path is None:
                raise BaseException('Error: can not find ".git" dir')

        print('Info: repo   :', repo_path)
        git_crypt_keyfile = os.path.join(repo_path, '.git', 'git-crypt', 'keys', 'default')
        if not os.path.isfile(git_crypt_keyfile):
            raise BaseException('Error: can not find git-crypt keyfile')
        print('Info: keyfile:', git_crypt_keyfile)

    # load keyfile
    f = open(git_crypt_keyfile, 'rb')
    d = f.read()
    f.close()

    if not d.startswith(b'\x00GITCRYPTKEY\x00'):
        raise BaseException('Error: invalid git-crypt keyfile')

    # skip magic
    data = d[0x20:]

    aes_key = None
    hmac_key= None
    while len(data) >= 8:
        block_id = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]
        block_len= (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7]
        if block_id == KEY_FIELD_AES_KEY:
            aes_key = data[8:8+block_len]
        elif block_id == KEY_FIELD_HMAC_KEY:
            hmac_key = data[8:8+block_len]
        data = data[8 + block_len:]

    if aes_key is None or hmac_key is None:
        raise BaseException('Error: invalid git-crypt keyfile')

    print('Info: AES  =', base64.b64encode(aes_key))
    print('Info: HMAC =', base64.b64encode(hmac_key))
    print()
    return {'AES': aes_key, 'HMAC': hmac_key}


def decrypt(keys, fn):
    f = open(fn, 'rb')
    d = f.read()
    d = d[10:] # skip magic header
    f.close()

    # AES CTR mode
    # block size 16bytes: 128bits
    # 12bytes noce, 4bytes BE format blockid
    nonce = d[:12]
    stream= d[12:]
    engine = AES.new(keys['AES'], AES.MODE_ECB)

    # decrypt data
    offs = 0
    out = []
    while True:
        block = stream[offs:offs+AES_BLOCK_LEN]

        # decrypt
        count = struct.pack('>I', offs//AES_BLOCK_LEN)
        ctr   = nonce + count
        mask  = engine.encrypt(ctr)
        n = len(block)
        for i in range(n):
            out.append(block[i] ^ mask[i])

        # prepare next block
        if len(block) < 16:
            break
        offs = offs + AES_BLOCK_LEN

    # check hash digest
    hmac_inst = HMAC.new(keys['HMAC'], digestmod=SHA1)
    hmac_inst.update(bytes(out))
    digest = hmac_inst.digest()

    if nonce[:HMAC_CHECK_LEN] == digest[:HMAC_CHECK_LEN]:
        return bytes(out)

    print('Error %s' % fn)
    print('  hmac expect:', base64.b64encode(nonce).decode('UTF-8'), '...')
    print('  hmac actual:', base64.b64encode(digest[:12]).decode('UTF-8'), '...')
    print()
    return None

def print_usage(fn):
    fn = os.path.split(fn)[1]
    print('usage: %s [options]' % fn)
    print('  --key file     force loading git-crypt keyfile from special file')
    print('  --all          search all files under work directory')


if __name__ == '__main__':
    opts, args = getopt.getopt(sys.argv[1:], 'h', ['help', 'key=', 'all'])
    m = {}
    for k, v in opts:
        m[k] = v

    if len(args) == 0 and '--all' not in m:
        print_usage(sys.argv[0])
        sys.exit(0)

    if '--help' in m or '-h' in m:
        print_usage(sys.argv[0])
        sys.exit(0)

    if '--key' in m:
        git_crypt_keyfile = m['--key']
        keys = load_keyfile(git_crypt_keyfile=git_crypt_keyfile)
    else:
        keys = load_keyfile()

    # search files
    encrypted_files = []
    if '--all' in m:
        for prefix, dirs, files in os.walk('.'):
            for fn in files:
                full_name = os.path.join(prefix, fn)
                f = open(full_name, 'rb')
                d = f.read(10)
                f.close()
                if d.startswith(b'\x00GITCRYPT\x00'):
                    encrypted_files.append(full_name)

    else:
        for pattern in args:
            for fn in glob.glob(pattern):
                if not os.path.isfile(fn):
                    continue
                f = open(fn, 'rb')
                d = f.read(10)
                f.close()

                if d.startswith(b'\x00GITCRYPT\x00'):
                    encrypted_files.append(fn)

    # try to decrypt
    nr_files = len(encrypted_files)
    nr_failures = 0
    for fn in encrypted_files:
        out = decrypt(keys, fn)
        if out is None:
            nr_failures = nr_failures + 1
            continue

        newfn = fn + '.gcrypt-plain'
        f = open(newfn, 'wb')
        f.write(out)
        f.close()
        print('Info: dump', newfn)

    if nr_files > 0:
        nr_pass = nr_files - nr_failures
        percent = 100.0 * nr_pass / nr_files
        print('Info: %6.2f%% %d/%d OK' % (percent, nr_pass, nr_files))
        if nr_failures > 0:
            percent = 100.0 - percent
            print('Info: %6.2f%% %d/%d Fail' % (percent, nr_failures, nr_files))
    else:
        print('Info: no git-crypt encrypted file found')

