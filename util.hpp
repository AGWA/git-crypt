#ifndef _UTIL_H
#define _UTIL_H

#include <string>

int		exec_command (const char* command, std::string& output);
std::string	resolve_path (const char* path);

#endif

