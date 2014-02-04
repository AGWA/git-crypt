CXX := c++
CXXFLAGS := -Wall -pedantic -ansi -Wno-long-long -O2
LDFLAGS := -lcrypto -lwsock32
PREFIX := /usr/local

OBJFILES = git-crypt.o commands.o crypto.o util.o util_win32.o

all: git-crypt

git-crypt: $(OBJFILES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o git-crypt

install:
	install -m 755 git-crypt $(PREFIX)/bin/

.PHONY: all clean install
