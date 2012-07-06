CXX := g++
CXXFLAGS := -Wall -pedantic -ansi -Wno-long-long -O2
LDFLAGS := -lcrypto

OBJFILES = git-crypt.o commands.o crypto.o util.o

all: git-crypt

git-crypt: $(OBJFILES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o git-crypt

.PHONY: all clean
