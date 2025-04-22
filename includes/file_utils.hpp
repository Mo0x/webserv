#ifndef FILE_UTILS_HPP
#define FILE_UTILS_HPP

#include <string>

bool fileExists(const std::string& path);
std::string readFile(const std::string& path);
bool isPathSafe(const std::string& base, const std::string& target);


#endif
