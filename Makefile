CXX := c++
CXXFLAGS := -Wall -pedantic -Wno-long-long -O2
LDFLAGS :=
PREFIX := /usr/local

OBJFILES = \
    git-crypt.o \
    commands.o \
    crypto.o \
    gpg.o \
    key.o \
    util.o \
    parse_options.o

OBJFILES += crypto-openssl.o
LDFLAGS += -lcrypto

all: git-crypt

git-crypt: $(OBJFILES)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJFILES) $(LDFLAGS)

util.o: util.cpp util-unix.cpp util-win32.cpp

clean:
	rm -f *.o git-crypt

install: git-crypt
	install -m 755 git-crypt $(DESTDIR)$(PREFIX)/bin/

.PHONY: all clean install
