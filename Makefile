#
# Copyright (c) 2015 Andrew Ayer
#
# See COPYING file for license information.
#

# Compiler Flags
CXXFLAGS ?= -Wall -pedantic -Wno-long-long -O2
CXXFLAGS += -std=c++11
# Added -Wno-deprecated-declarations to suppress deprecated OpenSSL warnings
CXXFLAGS += -Wno-deprecated-declarations

# Installation Directories
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man

# Documentation
ENABLE_MAN ?= no
DOCBOOK_XSL ?= http://cdn.docbook.org/release/xsl-nons/current/manpages/docbook.xsl

# Object Files
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

OBJFILES += crypto-openssl-11.o

OS ?= $(shell uname)

# Linker Flags
# Initially includes -lcrypto; additional flags will be appended based on OS
LDFLAGS += -lcrypto

# Detect Operating System
# $(OS) is typically 'Windows_NT' on Windows and empty or 'Linux' on Linux
ifeq ($(OS),Windows_NT)
    # Windows-specific linker flags
    LDFLAGS += -static-libstdc++ -static -lcrypto -lws2_32 -lcrypt32
endif
ifeq ($(OS),Darwin)
	LDFLAGS += "-L/opt/homebrew/opt/openssl@3/lib"
    CPPFLAGS += "-I/opt/homebrew/opt/openssl@3/include"
else
    # Unix/Linux-specific linker flags
    # LDFLAGS += -lpthread
endif

# Documentation Tools
XSLTPROC ?= xsltproc
DOCBOOK_FLAGS += --param man.output.in.separate.dir 1 \
		 --stringparam man.output.base.dir man/ \
		 --param man.output.subdirs.enabled 1 \
		 --param man.authors.section.enabled 1

# Targets
all: build

#
# Build
#
BUILD_MAN_TARGETS-yes = build-man
BUILD_MAN_TARGETS-no =
BUILD_TARGETS := build-bin $(BUILD_MAN_TARGETS-$(ENABLE_MAN))

build: $(BUILD_TARGETS)

build-bin: git-crypt

# Linking the binary
git-crypt: $(OBJFILES)
	@echo OS=$(OS) LDFLAGS=$(LDFLAGS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJFILES) $(LDFLAGS)

# Object File Dependencies
util.o: util.cpp util-unix.cpp util-win32.cpp
coprocess.o: coprocess.cpp coprocess-unix.cpp coprocess-win32.cpp

# Building Manual Pages
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
	rm -f $(OBJFILES) git-crypt git-crypt.exe

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
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 git-crypt $(DESTDIR)$(BINDIR)/

install-man: build-man
	install -d $(DESTDIR)$(MANDIR)/man1
	install -m 644 man/man1/git-crypt.1 $(DESTDIR)$(MANDIR)/man1/

.PHONY: all \
	build build-bin build-man \
	clean clean-bin clean-man \
	install install-bin install-man
