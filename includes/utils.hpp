#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>
#include "Config.hpp"

std::string to_string(size_t val);
const RouteConfig *findMatchingLocation(const ServerConfig &server, const std::string &path);
std::string generateAutoIndexPage(const std::string &dirPath, const std::string &uriPath);

#endif