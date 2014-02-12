CXX := c++
CXXFLAGS := -Wall -pedantic -ansi -Wno-long-long -O2
LDFLAGS := -lcrypto
PREFIX := /usr/local

OBJFILES = git-crypt.o commands.o crypto.o util.o

INSTALL = install -m 755 git-crypt $(PREFIX)/bin/

ifeq ($(OS),Windows_NT)
	LDFLAGS = -llibeay32 -lwsock32
#	CXXFLAGS += -static-libgcc
	CXXFLAGS += -static-libgcc -static-libstdc++
	OBJFILES = git-crypt.o commands.o crypto.o util_win32.o
	INSTALL = cp git-crypt.exe $(PREFIX)/bin/
endif

all: git-crypt

git-crypt: $(OBJFILES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o git-crypt git-crypt.exe \
	rm -fr test

install:
	$(INSTALL)

test:
	./test.sh

strip:
	strip git-crypt.exe

.PHONY: all clean install test strip
