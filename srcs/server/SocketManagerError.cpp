#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "SocketManager.hpp"
#include "file_utils.hpp"
#include "request_response_struct.hpp"
#include "utils.hpp"



Response SocketManager::makeHtmlError(int code, const std::string& reason, const std::string& html)
{

	Response r;
	r.status_code = code;
	r.status_message = reason;
	r.headers["Content-Type"] = "text/html; charset=utf-8";
	r.headers["Content-Length"] = to_string(html.size());
	r.body = html;
	return r;
}

Response SocketManager::makeConfigErrorResponse(const ServerConfig &server,
													const RouteConfig  *route,
													int                 code,
													const std::string  &reason,
													const std::string  &fallbackHtml)
{
	//find URI in server
	std::map<int, std::string>::const_iterator it = server.error_pages.find(code);
	if (it == server.error_pages.end())
	{
		return makeHtmlError(code, reason, fallbackHtml);
	}
	std::string uri = it->second;
	// chose root: route root or server root
	std::string root = server.root;
	if (route && !route->root.empty())
		root = route->root;
	//normalize slashes (/)
	if (!uri.empty() && uri[0] == '/')
		uri.erase(0,1);
	if (!root.empty() && root[root.size() - 1] == '/')
		root.erase(root.size() - 1);
	std::string fullPath = root + "/" + uri;

	//if file missing ->fallback makeHtmlError
	if (!fileExists(fullPath))
		return makeHtmlError(code, reason, fallbackHtml);
	// we read file and build Res
	std::string html = readFile(fullPath);
	if (html.empty())
		html = fallbackHtml;
	Response r;
	r.status_code = code;
	r.status_message = reason;
	r.headers["Content-Type"] = "text/html; charset=utf-8";
	r.headers["Content-Length"] = to_string(html.size());
	r.body = html;
	return r; 
}