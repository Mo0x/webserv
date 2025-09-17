#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>
#include "Config.hpp"

std::string to_string(size_t val);
const RouteConfig *findMatchingLocation(const ServerConfig &server, const std::string &path);
std::string generateAutoIndexPage(const std::string &dirPath, const std::string &uriPath);
std::string toUpperCopy(const std::string &str);
std::set<std::string> normalizeAllowedForAllowHeader(const std::set<std::string> &conf);
std::string joinAllowedMethods(const std::set<std::string> &methods);
bool isMethodAllowedForRoute(const std::string &methodUpper, const std::set<std::string> &allowed);

#endif