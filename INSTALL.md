### Dependencies

To build git-crypt, you need:

| Software                        | Debian/Ubuntu package | RHEL/CentOS package|
|---------------------------------|-----------------------|--------------------|
|Make                             | make                  | make               |
|A C++11 compiler (e.g. gcc 4.9+) | g++                   | gcc-c++            |
|OpenSSL development files        | libssl-dev            | openssl-devel      |

OpenSSL 1.1.1 or newer is required (OpenSSL 3.x and 4.x are supported).
git-crypt uses only OpenSSL's EVP interfaces, so it builds without
deprecation warnings against OpenSSL 3 and later.


To use git-crypt, you need:

| Software                        | Debian/Ubuntu package | RHEL/CentOS package|
|---------------------------------|-----------------------|--------------------|
|Git 1.7.2 or newer               | git                   | git                |
|OpenSSL                          | openssl               | openssl            |

Note: Git 1.8.5 or newer is recommended for best performance.


### Building git-crypt on Linux

With the distribution's OpenSSL development package installed, run:

    make
    make install

To install to a specific location:

    make install PREFIX=/usr/local

Or, just copy the git-crypt binary to wherever is most convenient for you.

To check the build, run `make test` (see [tests/README.md](tests/README.md)).

*Tested on Debian 12 (bookworm) with OpenSSL 3.0.  The Test workflow also
runs this recipe on ubuntu-latest.*


### Customizing the build

The Makefile honors the usual variables, from the environment or the command
line:

* `CPPFLAGS`: preprocessor flags, e.g. `-I` options to find OpenSSL's headers.
* `CXXFLAGS`: compiler flags.  Setting it replaces the default
  (`-Wall -pedantic -Wno-long-long -O2`), so prefer `CPPFLAGS` for include
  paths.
* `LDFLAGS`: linker flags.  When `LDFLAGS` comes from the *environment*, the
  Makefile appends `-lcrypto` (and, on Windows, the system libraries) to it.
  When it is given on the make *command line* (`make LDFLAGS=...`), it
  replaces everything, so you must include `-lcrypto` yourself.  Prefer the
  environment.


### Building on macOS

To install a released version, use Homebrew:

    brew install git-crypt

To build from source, install OpenSSL with Homebrew and point the build at it
(Homebrew doesn't put OpenSSL on the default search paths):

    brew install openssl
    CPPFLAGS="-I$(brew --prefix openssl)/include" LDFLAGS="-L$(brew --prefix openssl)/lib" make

*Tested on macOS (Apple Silicon) with Homebrew OpenSSL 4.1.  The Test workflow
also runs this recipe on macos-latest with Homebrew OpenSSL 3.*


### Building on Windows

git-crypt builds with MinGW-w64 in an [MSYS2](https://www.msys2.org/) shell.
In the "MSYS2 MINGW64" shell, install the toolchain and OpenSSL:

    pacman -S --needed base-devel git mingw-w64-x86_64-toolchain mingw-w64-x86_64-openssl

Then run:

    make

The Makefile detects Windows (MSYS2 sets `OS=Windows_NT`) and links the
Windows system libraries OpenSSL needs (`-lws2_32 -lcrypt32`).  The resulting
`git-crypt.exe` depends on MSYS2's DLLs.  To build a stand-alone executable,
link statically, as the release workflow does:

    LDFLAGS="-static-libstdc++ -static" make

Use `git-crypt.exe` with [Git for Windows](https://gitforwindows.org/), not
with MSYS2's `git` package: MSYS2's `git` prints POSIX paths (such as
`/tmp/...`) that `git-crypt.exe`, a native Windows program, can't open, so
`lock` and `unlock` don't find your working tree.  Link statically when using
Git for Windows, too: it runs git-crypt's filters with its own DLLs first on
`PATH`, and a dynamically linked `git-crypt.exe` fails to start.

*Not yet tested on a Windows machine.  The Test workflow builds with these
commands on windows-2022 and runs the integration scenarios with Git for
Windows; the Release workflow builds the same way.*

Cygwin is not tested or supported; use MSYS2.

Windows support is less tested than Unix support.  In particular, it does not
currently create key files with restrictive permissions, making it unsuitable
for use on a multi-user system.  Bug reports and patches are most welcome!


### Building a static binary on Linux

To build a git-crypt binary with no shared-library dependencies (for example
to copy it to machines without OpenSSL), link statically against the static
OpenSSL library (`libcrypto.a`, included in Debian's `libssl-dev`):

    LDFLAGS="-static" make

With glibc, the linker warns that some functions used inside libcrypto
(`dlopen`, `getaddrinfo`, `gethostbyname`) need glibc's shared libraries at
run time.  git-crypt doesn't use the OpenSSL features that call them, so the
binary works, but for a fully self-contained binary build on a musl-based
distribution such as Alpine Linux instead.

*Tested on Debian 12 (bookworm): `ldd` reports "not a dynamic executable" and
`make test` passes.  The Alpine variant is untested.*


### Building The Man Page

To build and install the git-crypt(1) man page, pass `ENABLE_MAN=yes` to make:

    make ENABLE_MAN=yes
    make ENABLE_MAN=yes install

xsltproc is required to build the man page.  Note that xsltproc will access
the Internet to retrieve its stylesheet unless the Docbook stylesheet is
installed locally and registered in the system's XML catalog.  To use a local
copy explicitly, set `DOCBOOK_XSL`, e.g. on Debian with the `docbook-xsl`
package:

    make ENABLE_MAN=yes DOCBOOK_XSL=/usr/share/xml/docbook/stylesheet/docbook-xsl/manpages/docbook.xsl


### Building A Debian Package

Debian packaging can be found in the 'debian' branch of the project Git
repository.  The package is built using git-buildpackage as follows:

    git checkout debian
    git-buildpackage -uc -us
