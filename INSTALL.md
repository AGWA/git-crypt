### Dependencies

To build git-crypt, you need:

| Software                        | Debian/Ubuntu package | RHEL/CentOS package|
|---------------------------------|-----------------------|--------------------|
|Make                             | make                  | make               |
|A C++11 compiler (e.g. gcc 4.9+) | g++                   | gcc-c++            |
|OpenSSL development files        | libssl-dev            | openssl-devel      |


To use git-crypt, you need:

| Software                        | Debian/Ubuntu package | RHEL/CentOS package|
|---------------------------------|-----------------------|--------------------|
|Git 1.7.2 or newer               | git                   | git                |
|OpenSSL                          | openssl               | openssl            |

Note: Git 1.8.5 or newer is recommended for best performance.


### Building git-crypt

Run:

    make
    make install

To install to a specific location:

    make install PREFIX=/usr/local

Or, just copy the git-crypt binary to wherever is most convenient for you.


### Building The Man Page

To build and install the git-crypt(1) man page, pass `ENABLE_MAN=yes` to make:

    make ENABLE_MAN=yes
    make ENABLE_MAN=yes install

xsltproc is required to build the man page.  Note that xsltproc will access
the Internet to retrieve its stylesheet unless the Docbook stylesheet is
installed locally and registered in the system's XML catalog.


### Building A Debian Package

Debian packaging can be found in the 'debian' branch of the project Git
repository.  The package is built using git-buildpackage as follows:

    git checkout debian
    git-buildpackage -uc -us


### Installing On Mac OS X

Using the brew package manager, simply run:

    brew install git-crypt

### Experimental Windows Support

git-crypt should build on Windows with MinGW, although the build system
is not yet finalized so you will need to pass your own CXX, CXXFLAGS, and
LDFLAGS variables to make.  Additionally, Windows support is less tested
and does not currently create key files with restrictive permissions,
making it unsuitable for use on a multi-user system.  Windows support
will mature in a future version of git-crypt.  Bug reports and patches
are most welcome!
