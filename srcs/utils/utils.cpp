#include "utils.hpp"
#include <sstream>

std::string to_string(size_t val)
{
	std::ostringstream oss;
	oss << val;
	return oss.str();
}

/* Give this longest prefix IE: location /upload is taken vs location /*/

const RouteConfig* findMatchingLocation(const ServerConfig& server, const std::string& path) {
	const RouteConfig* bestMatch = NULL;
	size_t bestLength = 0;

	for (size_t i = 0; i < server.routes.size(); ++i) {
		const RouteConfig& route = server.routes[i];
		if (path.find(route.path) == 0 && route.path.length() > bestLength) {
			bestMatch = &route;
			bestLength = route.path.length();
		}
	}
	return bestMatch;
}
