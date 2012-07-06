#ifndef _COMMANDS_H
#define _COMMANDS_H


void clean (const char* keyfile);
void smudge (const char* keyfile);
void diff (const char* keyfile, const char* filename);
void init (const char* argv0, const char* keyfile);
void keygen (const char* keyfile);

#endif

