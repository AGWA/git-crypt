#include "commands.hpp"
#include "util.hpp"
#include <cstring>
#include <iostream>

static void print_usage (const char* argv0)
{
	std::clog << "Usage: " << argv0 << " COMMAND [ARGS ...]\n";
	std::clog << "\n";
	std::clog << "Valid commands:\n";
	std::clog << " init KEYFILE   - prepare the current git repo to use git-crypt with this key\n";
	std::clog << " keygen KEYFILE - generate a git-crypt key in the given file\n";
	std::clog << "\n";
	std::clog << "Plumbing commands (not to be used directly):\n";
	std::clog << " clean KEYFILE\n";
	std::clog << " smudge KEYFILE\n";
	std::clog << " diff KEYFILE FILE\n";
}


int main (int argc, const char** argv)
{
	// The following two lines are essential for achieving good performance:
	std::ios_base::sync_with_stdio(false);
	std::cin.tie(0);

	if (argc < 3) {
		print_usage(argv[0]);
		return 2;
	}


	if (strcmp(argv[1], "init") == 0 && argc == 3) {
		init(argv[0], argv[2]);
	} else if (strcmp(argv[1], "keygen") == 0 && argc == 3) {
		keygen(argv[2]);
	} else if (strcmp(argv[1], "clean") == 0 && argc == 3) {
		clean(argv[2]);
	} else if (strcmp(argv[1], "smudge") == 0 && argc == 3) {
		smudge(argv[2]);
	} else if (strcmp(argv[1], "diff") == 0 && argc == 4) {
		diff(argv[2], argv[3]);
	} else {
		print_usage(argv[0]);
		return 2;
	}

	return 0;
}


