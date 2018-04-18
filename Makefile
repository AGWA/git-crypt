#
# Copyright (c) 2015 Andrew Ayer
#
# See COPYING file for license information.
#

CXXFLAGS ?= -Wall -pedantic -Wno-long-long -O2
CXXFLAGS += -std=c++11
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man

ifeq ($(OS),Windows_NT)
# MSys2 install instead of ./INSTALL
INSTALL = /usr/bin/install
else
INSTALL = install
endif

ENABLE_MAN ?= no
DOCBOOK_XSL ?= http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl

$(info OSTYPE=$(OSTYPE))

OBJFILES = \
    git-crypt.o \
    commands.o \
    crypto.o \
    gpg.o \
    key.o \
    util.o \
    parse_options.o \
    coprocess.o \
    fhstream.o

OBJFILES += crypto-openssl-10.o crypto-openssl-11.o
LDFLAGS += -lcrypto

XSLTPROC ?= xsltproc
DOCBOOK_FLAGS += --param man.output.in.separate.dir 1 \
		 --stringparam man.output.base.dir man/ \
		 --param man.output.subdirs.enabled 1 \
		 --param man.authors.section.enabled 1

all: build

#
# Build
#
BUILD_MAN_TARGETS-yes = build-man
BUILD_MAN_TARGETS-no =
BUILD_TARGETS := build-bin $(BUILD_MAN_TARGETS-$(ENABLE_MAN))

build: $(BUILD_TARGETS)

build-bin: git-crypt

git-crypt: $(OBJFILES)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJFILES) $(LDFLAGS)

util.o: util.cpp util-unix.cpp util-win32.cpp
coprocess.o: coprocess.cpp coprocess-unix.cpp coprocess-win32.cpp

build-man: man/man1/git-crypt.1

man/man1/git-crypt.1: man/git-crypt.xml
	$(XSLTPROC) $(DOCBOOK_FLAGS) $(DOCBOOK_XSL) man/git-crypt.xml

#
# Clean
#
CLEAN_MAN_TARGETS-yes = clean-man
CLEAN_MAN_TARGETS-no =
CLEAN_TARGETS := clean-bin $(CLEAN_MAN_TARGETS-$(ENABLE_MAN))

clean: $(CLEAN_TARGETS)

clean-bin:
	rm -f $(OBJFILES) git-crypt

clean-man:
	rm -f man/man1/git-crypt.1

#
# Install
#
INSTALL_MAN_TARGETS-yes = install-man
INSTALL_MAN_TARGETS-no =
INSTALL_TARGETS := install-bin $(INSTALL_MAN_TARGETS-$(ENABLE_MAN))

install: $(INSTALL_TARGETS)

install-bin: build-bin
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 git-crypt $(DESTDIR)$(BINDIR)/

install-man: build-man
	$(INSTALL) -d $(DESTDIR)$(MANDIR)/man1
	$(INSTALL) -m 644 man/man1/git-crypt.1 $(DESTDIR)$(MANDIR)/man1/

.PHONY: all \
	build build-bin build-man \
	clean clean-bin clean-man \
	install install-bin install-man
