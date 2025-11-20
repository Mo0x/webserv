#ifndef FILE_UTILS_HPP
#define FILE_UTILS_HPP

#include <string>
#include <sys/stat.h>

bool fileExists(const std::string& path);
std::string readFile(const std::string& path);
bool isPathSafe(const std::string& base, const std::string& target);
bool isPathTraversalSafe(const std::string &path);
bool dirExists(const std::string& path);

#endif
