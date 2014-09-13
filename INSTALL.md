Dependencies
------------

To use git-crypt, you need:

*   Git 1.6.0 or newer
*   OpenSSL
*   For decrypted git diff output, Git 1.6.1 or newer
*   For decrypted git blame output, Git 1.7.2 or newer

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
