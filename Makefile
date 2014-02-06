CXX := c++
CXXFLAGS := -Wall -pedantic -ansi -Wno-long-long -O2
LDFLAGS := -lcrypto
PREFIX := /usr/local

OBJFILES = git-crypt.o commands.o crypto.o util.o

ifeq ($(OS),Windows_NT)
	LDFLAGS = -llibeay32 -lwsock32
	OBJFILES = git-crypt.o commands.o crypto.o util_win32.o
endif

all: git-crypt

git-crypt: $(OBJFILES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o git-crypt git-crypt.exe

install:
	install -m 755 git-crypt $(PREFIX)/bin/

.PHONY: all clean install
