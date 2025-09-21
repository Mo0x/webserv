#include "utils.hpp"
#include <sstream>
#include <dirent.h>

std::string to_string(size_t val)
{
	std::ostringstream oss;
	oss << val;
	return oss.str();
}

bool isPathPrefix(const std::string& path, const std::string& prefix) {
	if (path.find(prefix) != 0)
		return false;

	if (path.length() == prefix.length())
		return true;

	// Make sure next char is '/' to avoid matching "/imageshack" with "/images"
	return path[prefix.length()] == '/';
}

/* Give this longest prefix IE: location /upload is taken vs location /*/

const RouteConfig* findMatchingLocation(const ServerConfig& server, const std::string& path) {
	const RouteConfig* bestMatch = NULL;
	size_t bestLength = 0;

	for (size_t i = 0; i < server.routes.size(); ++i) {
		const RouteConfig& route = server.routes[i];
		const std::string& locPath = route.path;

		if (path.compare(0, locPath.size(), locPath) == 0) {
			bool exact = path.size() == locPath.size();
			bool nextCharIsSlash = path.size() > locPath.size() && path[locPath.size()] == '/';
			bool locEndsWithSlash = !locPath.empty() && locPath[locPath.size() - 1] == '/';

			if (exact || nextCharIsSlash || locEndsWithSlash) {
				if (locPath.size() > bestLength) {
					bestLength = locPath.size();
					bestMatch = &route;
				}
			}
		}
	}
	return bestMatch;
}

/* THIS WORKS ON UNIX BUT NOT MAC BECAUSE IN THAT CASE d_type is alway UNKNOWN*/
std::string generateAutoIndexPage(const std::string &dirPath, const std::string &uriPath)
{
	std::ostringstream html;
	html << "<html><head><title> Index of " << uriPath << "</title></head><body>";
	html << "<h1>Index of " << uriPath << "</h1><ul>";
	DIR *dir = opendir(dirPath.c_str());
	if (!dir)
		return "<h1>403 Forbidden</h1>";
	
	struct dirent* entry;
	while((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;
		if (name == "." || name == "..") // <---- My choice to forbid ".." normally not a problem because path validation is clean but could lead to problem with weird dirs/config I.E directory traversing to a "admin directory"
			continue ;
		html << "<li> <a href=\"" << uriPath;
		if (uriPath[uriPath.size() - 1] != '/') 
			html << "/";
		html << name << "\">" << name << "</a> </li";
	}
	closedir(dir);
	html << "</ul></body></html>";
	return html.str();
}

std::string toUpperCopy(const std::string& str)
{
    std::string ret(str);
    for (size_t i = 0; i < ret.size(); ++i)
        ret[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(ret[i])));
    return ret;
}

// if GET is allowed, we must allow the HEAD method too : they share the same semantics; HEAD just omits the body
std::set<std::string> normalizeAllowedForAllowHeader(const std::set<std::string> &conf)
{
	std::set<std::string> out = conf;
	if (out.find("GET") != out.end())
		out.insert("HEAD");
	return out;
}

std::string joinAllowedMethods(const std::set<std::string> &methods)
{
	std::string out;
	for (std::set<std::string>::const_iterator it = methods.begin(); it != methods.end(); it++)
	{
		if (!out.empty())
			out += ", ";
		out += *it;
	}
	return out;
}

bool isMethodAllowedForRoute(const std::string &methodUpper, const std::set<std::string> &allowed)
{
	if (methodUpper == "HEAD")
		return (allowed.find("GET") != allowed.end());
	return (allowed.find(methodUpper) != allowed.end());
}