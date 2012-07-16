#ifndef _UTIL_H
#define _UTIL_H

#include <string>
#include <ios>
#include <iosfwd>

int		exec_command (const char* command, std::string& output);
std::string	resolve_path (const char* path);
void		open_tempfile (std::fstream&, std::ios_base::openmode);

#endif

