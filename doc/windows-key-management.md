# Windows Key File Protection

Windows key files are created with an explicit, protected DACL granting full
access only to the current token's user SID. Parent-directory permissions are
not inherited. Existing files are opened without truncation, checked for current
user ownership and a single hard link, and given the same protected DACL. The
DACL is read back and checked before any key bytes are written. Filesystems
that cannot enforce this protection cause an error rather than an unprotected
write. Reparse points at the final path component and directories are rejected.

## Transactional Writes

Key storage writes to a new file in the same directory, using a name generated
with the Windows cryptographic random generator and `CREATE_NEW`. The temporary
file has a protected DACL from creation, is held without sharing, and is never
closed and reopened by path. Name collisions are retried without opening or
modifying the colliding file. Only after all bytes have been written and
`FlushFileBuffers` succeeds is the file renamed over the target with
`SetFileInformationByHandle`. The protected temporary file's DACL is retained.
There is no delete-before-replace or cross-volume copy fallback.

An existing target is held throughout validation and commit with read/write
sharing denied and delete sharing allowed for replacement. Conflicting open
handles, read-only files, foreign owners, hard links and final-component reparse
points are refused. Existing files owned by another SID include files whose
owner is an administrator group rather than the current user's SID. If no target
existed when checked, the commit refuses to replace a newly appearing target.

Write, flush and rename failures before commit preserve the original key bytes.
Temporary files are marked for deletion by their open handle on normal exception
paths; cleanup failures can leave a file with the restrictive DACL. A target's
DACL may have been tightened even if its contents were not replaced. Securing
an existing file cannot revoke access to bytes already disclosed.

Successful rename is the commit boundary. Errors after it, such as a failure
closing the handle, do not roll back the committed key. Process termination can
leave an orphaned protected temporary file. Flushing file data and using a
same-volume rename do not guarantee durable directory metadata or crash
consistency on every filesystem, device or network share. Keep a separate secure
backup; this is not Windows Transactional NTFS (TxF).

The general rename utility still uses `MoveFileExA` with replacement and
write-through, without pre-deleting the target or permitting cross-volume copy.

## Remaining Boundaries

This protection assumes trusted parent directories and ordinary local Windows
filesystem semantics. It does not prevent an attacker with directory-control
rights from redirecting ancestor path components or subsequently replacing or
deleting directory entries. Protect the repository and export directories too.
Administrators, backup privileges, compromised accounts, network filesystems
and physical access are outside the protection promised by a user-only DACL.
ANSI path handling is unchanged; this change does not add general Unicode path
support or alter the key/encryption format. The handle-based rename uses a wide
Windows API with conversion of the existing ANSI path; it is not general Unicode
path support.

## Validation

The Windows PR workflow retains MSYS2/MinGW and static OpenSSL linkage. Run
`make test-windows` with the same linker flags as the Windows release build.
The tests cover protected creation under a permissive parent, tightening an
existing DACL, format round trips, shorter overwrites, conflicting handles,
read-only files, invalid paths, hard links and rename success/failure. The test
binary compiles the production utility implementation with test-only Win32 I/O
wrappers. Injected partial-write, flush and rename failures check original-key
preservation and temporary-file cleanup for both new and existing destinations.
Additional tests check file identity changes on commit, temporary-name collision
retries and refusal to overwrite a competing newly created target. These wrappers
and fault controls are not compiled into the application binary. Symlink
rejection is tested when the runner has Developer Mode or symlink privileges;
otherwise that case is explicitly reported as skipped. Native Windows tests
are needed: a Linux build or cross-compilation alone does not validate DACLs.