# How git-crypt works

This document describes the on-disk format of git-crypt encrypted files and
key files, the cryptographic design, and how to decrypt data by hand. The
examples were produced with the current development version of git-crypt.

## Overview

git-crypt is a Git clean/smudge filter. When you `git add` a file matched by
a `filter=git-crypt` attribute, `git-crypt clean` encrypts it, and the
encrypted bytes are what Git stores as the blob. On checkout,
`git-crypt smudge` decrypts the blob back into the work tree. The work tree
holds plaintext, but Git's object database (and therefore every clone and
push) only ever contains the encrypted bytes. The filters only work while
the repository is unlocked, i.e. while the symmetric key is installed in
`git-crypt/keys/` under the repository's common Git directory (usually
`.git/git-crypt/keys/`).

The same symmetric key is also stored in the repository, encrypted to each
collaborator's GPG key, when you use `git-crypt add-gpg-user`.

## Encrypted file format

An encrypted blob is exactly:

| Offset | Length | Contents |
|--------|--------|----------|
| 0      | 10     | Header: `\0GITCRYPT\0` (`00 47 49 54 43 52 59 50 54 00`) |
| 10     | 12     | Nonce (first 12 bytes of the HMAC-SHA1 of the plaintext) |
| 22     | N      | Ciphertext, same length as the plaintext |

Total size is `22 + N` bytes. The 17-byte file `hello, git-crypt\n` becomes
a 39-byte blob:

```
$ git cat-file blob HEAD:secret.txt | xxd
00000000: 0047 4954 4352 5950 5400 5680 4db8 157c  .GITCRYPT.V.M..|
00000010: 1b81 a70f e8d2 e3d7 8aa9 8e74 497c be19  ...........tI|..
00000020: a712 dc81 eafd 61                        ......a
```

Bytes 0-9 are the header, bytes 10-21 (`5680 4db8 157c 1b81 a70f e8d2`) are the
nonce, and bytes 22-38 are the ciphertext.

An empty file encrypts to a 22-byte blob: the header and the nonce, with no
ciphertext. (Currently git-crypt does this rather than storing an empty blob.)

The blob contains no key name or key version; see
[Limitations](#limitations).

## Encryption scheme

The scheme is AES-256 in CTR mode with a synthetic IV, in the same spirit as
SIV:

1. `nonce` = the first 12 bytes of `HMAC-SHA1(hmac_key, plaintext)`
   (`hmac_key` is 64 bytes).
2. The CTR counter block for the *i*-th 16-byte block (*i* counting from 0)
   is `nonce || be32(i)`: the 12-byte nonce followed by the 32-bit
   big-endian block number.
3. `keystream_i = AES-256(aes_key, counter_block_i)`, and the ciphertext is
   the plaintext XORed with the keystream. The final partial block uses only
   as many keystream bytes as needed, which is why the ciphertext is exactly
   as long as the plaintext.

Decryption (`git-crypt smudge`, `git-crypt diff`) runs CTR with the stored
nonce, then recomputes the HMAC-SHA1 of the decrypted plaintext and compares
its first 12 bytes with the stored nonce. A mismatch is reported as
`encrypted file has been tampered with!` and a non-zero exit status.
Note that the (garbled) plaintext has already been written to stdout by
then; the exit status is what tells Git not to use it.

Input that does not start with the header is passed through unchanged
(`smudge` and `diff` treat it as a file that was never encrypted).

### Why the encryption is deterministic

The nonce is derived from the content, so encrypting the same file with the
same key always gives the same blob. This is deliberate: Git decides whether
a file changed by comparing hashes, so encryption must be deterministic or
every `git status` would see every encrypted file as modified. It also lets
Git deduplicate identical blobs and show a file as unchanged.

Using an HMAC (rather than a plain hash) keeps the nonce, which has to be
stored in the clear, from being used to build a dictionary of plaintext
hashes. Since the nonce changes completely when any byte of the file changes,
two different plaintexts never share a counter block sequence, and a nonce is
reused only when the entire file is identical. This construction is
semantically secure under deterministic chosen-plaintext attack; it leaks
only whether two files are identical (see the README "Security" section and
the comment in `clean()` in `commands.cpp`).

`clean` refuses files of 2^32 blocks (64 GiB) or more, to avoid wrapping the
32-bit counter.

## Key files

### Symmetric key file format

This is the format of `git-crypt export-key`, of `--key-file` arguments, and
of `.git/git-crypt/keys/<name>` (`default` for the default key). All integers
are big-endian 32-bit.

| Part | Bytes | Meaning |
|------|-------|---------|
| Magic | 12 | `\0GITCRYPTKEY` |
| Format version | 4 | `2` |
| Header fields | variable | Zero or more `id(4) len(4) value(len)`, terminated by `id = 0` (`HEADER_FIELD_END`) |
| Key entries | variable | One or more entries, newest first |

Header field IDs: `1` = key name (`HEADER_FIELD_KEY_NAME`, at most 128 bytes,
omitted for the default key), `0` = end.

Each entry is a sequence of `id(4) len(4) value(len)` fields, terminated by
`id = 0` (`KEY_FIELD_END`):

| ID | Name | Length | Value |
|----|------|--------|-------|
| 1 | `KEY_FIELD_VERSION` | 4 | key version (be32), starting at 0 |
| 3 | `KEY_FIELD_AES_KEY` | 32 | AES-256 key |
| 5 | `KEY_FIELD_HMAC_KEY` | 64 | HMAC-SHA1 key |
| 0 | `KEY_FIELD_END` | none | ends the entry (no length field) |

Odd field IDs are "critical": a reader that does not recognize an odd ID
must refuse the file. Unrecognized even IDs are skipped using their length.

A key file for the default key with one version is exactly 148 bytes:

```
$ xxd k.key
00000000: 0047 4954 4352 5950 544b 4559 0000 0002  .GITCRYPTKEY....
00000010: 0000 0000 0000 0001 0000 0004 0000 0000  ................
00000020: 0000 0003 0000 0020 18b0 8c13 0f7f d69e  ....... ........
00000030: 21d0 b0da 7ae1 7406 d57d fbe3 46c2 c62c  !...z.t..}..F..,
00000040: 5b2c c473 0b03 540e 0000 0005 0000 0040  [,.s..T........@
00000050: 35cd 8829 74cd f887 bcc9 c0f7 5127 61fa  5..)t.......Q'a.
00000060: 163c cbd9 0157 da26 f19e c5ea 03b1 4603  .<...W.&......F.
00000070: e249 46cf dfd5 daf8 886c 0881 45bc b707  .IF......l..E...
00000080: fa6e 2e59 420c 1ee7 c5eb 01f9 8de3 ad8b  .n.YB...........
00000090: 0000 0000                                ....
```

Offsets: magic 0-11; format version 12-15 (`00000002`); header end 16-19
(`00000000`, no key name); entry starts at 20 with field 1 (id 20-23, len
24-27, version 28-31); field 3 (id 32-35, len 36-39, **AES key 40-71**);
field 5 (id 72-75, len 76-79, **HMAC key 80-143**); entry end 144-147.

A key file for a named key (`git-crypt init -k team`) has the name in the
header, so everything shifts by 12 bytes:

```
00000000: 0047 4954 4352 5950 544b 4559 0000 0002  .GITCRYPTKEY....
00000010: 0000 0001 0000 0004 7465 616d 0000 0000  ........team....
00000020: 0000 0001 0000 0004 0000 0000 0000 0003  ................
```

(The recipes below that extract keys with fixed offsets assume the default
key, offsets 40 and 80.)

### GPG-wrapped copies

With `git-crypt add-gpg-user`, git-crypt writes, for the key's latest version
and each collaborator:

```
.git-crypt/keys/<key name or "default">/<key version>/<GPG FINGERPRINT>.gpg
```

Each file is a standard OpenPGP message: a key file (format above) containing
exactly one key entry (that version), encrypted with `gpg` to that
collaborator. `git-crypt unlock` with no argument finds your secret key,
decrypts the matching file(s) and installs the result in
`.git/git-crypt/keys/`. The files are committed to the repository (the
`.git-crypt/` directory has its own `.gitattributes` marking `*.gpg` as
binary).

The key that is actually used for the filters lives (unencrypted) in
`.git/git-crypt/keys/<name>` in an unlocked repository.

## Manual decryption recipes

All recipes below were run against a scratch repository containing
`secret.txt` (attribute `filter=git-crypt diff=git-crypt`) committed twice,
with `HEAD~1` holding `hello, git-crypt` and `HEAD` holding
`hello, git-crypt (revised)`.

### 1. View an old version, decrypted (unlocked repository)

`--textconv` runs the repository's `diff=git-crypt` textconv driver on the
blob:

```
$ git show --textconv HEAD~1:secret.txt
hello, git-crypt
```

Without `--textconv`, `git show` prints the raw encrypted blob.

### 2. Decrypt one blob with `smudge`

In an unlocked repository, pipe the blob through `git-crypt smudge`:

```
$ git cat-file blob HEAD~1:secret.txt | git-crypt smudge
hello, git-crypt
$ git show HEAD~1:secret.txt | git-crypt smudge
hello, git-crypt
```

To use a specific key file instead of the repository's installed key, for
example one from `git-crypt export-key`:

```
$ git-crypt export-key ../k.key
$ git cat-file blob HEAD~1:secret.txt | git-crypt smudge --key-file=../k.key
hello, git-crypt
```

For a named key, pass `--key-name=NAME` (or `-k NAME`) instead of
`--key-file` to select the installed key of that name.

### 3. Decrypt a collaborator's `.gpg` file by hand

With the matching GPG secret key available, decrypt the wrapped key and use
the result as a key file:

```
$ gpg --decrypt --output alice.key \
    .git-crypt/keys/default/0/ADD0FD2156CE73907FC98432971AC44BE818599A.gpg
gpg: encrypted with cv25519 key, ID F352C02FA296345C, created 2026-11-11
      "Alice <alice@example.com>"
$ cmp alice.key k.key && echo identical
identical
$ git-crypt smudge --key-file=alice.key < blob.bin
hello, git-crypt
```

(`alice.key` is byte-identical to the `export-key` output here because the
repository has only key version 0.) Treat `alice.key` as a secret and delete
it when done.

### 4. Decrypting outside a Git work tree

`git-crypt smudge --key-file=KEYFILE` does not need a Git repository at all.
It was verified in a plain directory that is not inside any repository
(`git rev-parse --is-inside-work-tree` fails there), and also with
`GIT_DIR=/nonexistent`:

```
$ git rev-parse --is-inside-work-tree
fatal: not a git repository (or any of the parent directories): .git
$ git-crypt smudge --key-file=../k.key < blob.bin
hello, git-crypt
```

This makes it possible to decrypt a blob saved with `git cat-file blob` on
a machine that has only the key file and git-crypt. Without `--key-file` (or
`--key-name`) git-crypt looks for the key inside the repository's `.git`
directory, so that form does need a repository.

### 5. Decrypt with only the `openssl` CLI

Given the key file, you can reproduce the decryption without git-crypt. This
example uses the default key layout (AES key at offset 40, HMAC key at offset
80) and a blob in `blob.bin`:

```
# AES key (32 bytes at offset 40) and HMAC key (64 bytes at offset 80), as hex
AES=$(dd if=k.key bs=1 skip=40 count=32 2>/dev/null | xxd -p -c 64)
HMK=$(dd if=k.key bs=1 skip=80 count=64 2>/dev/null | xxd -p -c 128)
# Nonce: 12 bytes at offset 10 of the blob
NONCE=$(dd if=blob.bin bs=1 skip=10 count=12 2>/dev/null | xxd -p -c 24)

# Ciphertext starts at offset 22; IV = nonce || 00000000 (32-bit counter 0)
dd if=blob.bin bs=1 skip=22 2>/dev/null |
  openssl enc -d -aes-256-ctr -K "$AES" -iv "${NONCE}00000000"
```

Output: `hello, git-crypt`. This also worked for a 100,000-byte file (multiple
blocks; the CLI increments the low bytes of the counter block as big-endian).

To perform the integrity check, pipe the plaintext through the same HMAC
and compare the first 24 hex digits (12 bytes) with `$NONCE`:

```
... | openssl dgst -sha1 -mac HMAC -macopt hexkey:"$HMK"
SHA1(stdin)= e4cc53d4916ef4b27ace2d2bce616f88dca0054c
$ echo $NONCE
e4cc53d4916ef4b27ace2d2b
```

(Tested with OpenSSL 4.1.0. The `-mac HMAC` option may not exist in other
`openssl` builds such as LibreSSL.)

## Limitations

- **Deterministic.** The same plaintext always encrypts to the same blob (with
  a given key). Anyone with the repository can tell when a file did or did
  not change, and whether two files are identical.
- **File length is not hidden.** The ciphertext is exactly as long as the
  plaintext (plus 22 bytes), and an empty file is visible as a 22-byte blob.
- **No key version in the blob.** The blob does not record which key version
  encrypted it. The current implementation always decrypts with the key
  entry of version 0 (`key_version = 0` in `decrypt_file_to_stdout()`, with a
  TODO). Newer key versions are written to the key files but cannot be
  selected by a blob.
- **Integrity is a 96-bit truncated HMAC-SHA1**, and is checked only after
  the plaintext has been written to stdout.
- File names, commit messages, symlink targets, and other metadata are not
  encrypted; see the README "Limitations" section.
