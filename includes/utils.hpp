#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>
#include "Config.hpp"
#include "request_reponse_struct.hpp"

std::string to_string(size_t val);
const RouteConfig *findMatchingLocation(const ServerConfig &server, const std::string &path);
std::string generateAutoIndexPage(const std::string &dirPath, const std::string &uriPath);
std::string toUpperCopy(const std::string &str);
std::set<std::string> normalizeAllowedForAllowHeader(const std::set<std::string> &conf);
std::string joinAllowedMethods(const std::set<std::string> &methods);
bool isMethodAllowedForRoute(const std::string &methodUpper, const std::set<std::string> &allowed);
bool shouldCloseAfterThisResponse(int status, bool headers_complete, bool body_was_expected, bool body_fully_consumed, bool client_said_close);
bool clientRequestedClose(const Request& req);
std::string getMimeTypeFromPath(const std::string& path);
void countHeaderNames(const std::string &rawHeaders, std::map<std::string, size_t> &outCounts);

#endif