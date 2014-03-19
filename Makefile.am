
##
## General
##

bin_PROGRAMS = git-crypt

git_crypt_SOURCES = \
	commands.cpp commands.hpp \
	crypto.cpp crypto.hpp \
	git-crypt.cpp \
	util.cpp util.hpp

git_crypt_CPPFLAGS = \
	-Wall -pedantic -ansi -Wno-long-long -O2 $(OPENSSL_CFLAGS)

git_crypt_LDADD = \
	$(OPENSSL_LIBS)

##
## Debian Package
##

##
## Prerequisites:
##
## sudo apt-get install build-essential autotools-dev dh-make debhelper devscripts fakeroot xutils lintian pbuilder
##

git_user=$(shell git config user.name)
git_email=$(shell git config user.email)

cfg_line=$(shell sed -n 's/ac_cs_config="\(.*\)"/\1/p' $(abs_builddir)/config.status)

pkg-debian: dist-gzip
	GZIP=$(GZIP_ENV) gzip -dc $(distdir).tar.gz | $(am__untar) && \
	cd $(distdir) && \
	echo | dh_make -s -e `git config user.email` -f ../$(distdir).tar.gz && \
	cp $(abs_srcdir)/debian/control debian/control && \
	cp $(abs_srcdir)/debian/changelog debian/changelog && \
	cp $(abs_srcdir)/debian/copyright debian/copyright && \
	rm debian/README.Debian debian/README.source debian/*.ex debian/*.EX && \
	sed -i 's/Maintainer:.*/Maintainer: $(git_user) <$(git_email)>/g' debian/control && \
	echo -e '\noverride_dh_auto_configure:\n\t./configure $(cfg_line)' >> debian/rules && \
	echo -e '\noverride_dh_usrlocal:' >> debian/rules && \
	dpkg-buildpackage -rfakeroot -us -uc

##
## Clean
##

clean-local:
	rm -rf $(PACKAGE)_$(VERSION)* $(PACKAGE)-$(VERSION)*

distclean-local:
	rm -rf $(LOCAL_DESTDIR)

