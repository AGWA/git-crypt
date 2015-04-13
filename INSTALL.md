Dependencies
------------

To use git-crypt, you need:

*   Git 1.7.2 or newer
*   OpenSSL

To build git-crypt, you need a C++ compiler and OpenSSL development
headers.


Building git-crypt
------------------

The Makefile is tailored for g++, but should work with other compilers.

    make
    cp git-crypt /usr/local/bin/

It doesn't matter where you install the git-crypt binary - choose
wherever is most convenient for you.


Building A Debian Package
-------------------------

Debian packaging can be found in the 'debian' branch of the project Git
repository.  The package is built using git-buildpackage as follows:

    git checkout debian
    git-buildpackage -uc -us


Installing On Mac OS X
----------------------

Using the brew package manager, simply run:

    brew install git-crypt

Red Hat Enterprise Linux
------------------------

In addition to installing git and openssl, you will need the following
support packages:

yum install perl-CPAN
yum install gettext-devel
yum install expat-devel
yum install openssl-devel

Experimental Windows Support
----------------------------

git-crypt should build on Windows with MinGW, although the build system
is not yet finalized so you will need to pass your own CXX, CXXFLAGS, and
LDFLAGS variables to make.  Additionally, Windows support is less tested
and does not currently create key files with restrictive permissions,
making it unsuitable for use on a multi-user system.  Windows support
will mature in a future version of git-crypt.  Bug reports and patches
are most welcome!
