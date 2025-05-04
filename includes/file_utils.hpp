#ifndef FILE_UTILS_HPP
#define FILE_UTILS_HPP

#include <string>

bool fileExists(const std::string& path);
std::string readFile(const std::string& path);

#endif
