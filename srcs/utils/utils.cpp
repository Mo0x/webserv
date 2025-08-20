#include "utils.hpp"
#include <sstream>

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