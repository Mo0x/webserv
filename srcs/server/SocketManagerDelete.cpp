#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <cstdio>

#include "SocketManager.hpp"
#include "file_utils.hpp"
#include "request_response_struct.hpp"
#include "utils.hpp"

void SocketManager::handleDelete(int fd,
								 const Request &req,
								 const ServerConfig &server,
								 const RouteConfig *route)
{
	// If route not supplied, locate it
	const RouteConfig *r = route ? route : findMatchingLocation(server, req.path);

	// Check method allowed if we have a route
	if (r)
	{
		if (!isMethodAllowedForRoute("DELETE", r->allowed_methods))
		{
			 Response res = makeConfigErrorResponse(server, r, 405, "Method Not Allowed", "<h1>405 Method Not Allowed</h1>");

			res.headers["Allow"] = joinAllowedMethods(normalizeAllowedForAllowHeader(r->allowed_methods));

			finalizeAndQueue(fd, req, res,
							/*body_expected=*/false,
							/*body_fully_consumed=*/true);
			return;
		}
	}

	std::string effectiveRoot  = (r && !r->root.empty())  ? r->root  : server.root;

	// strip route prefix
	std::string strippedPath = req.path;
	if (r && strippedPath.find(r->path) == 0)
		strippedPath = strippedPath.substr(r->path.length());
	if (!strippedPath.empty() && strippedPath[0] != '/')
		strippedPath = "/" + strippedPath;

	std::string fullPath = effectiveRoot + strippedPath;

	// Safety checks
	if (!isPathSafe(effectiveRoot, fullPath))
	{
		Response res = makeConfigErrorResponse(server, r, 403, "Forbidden", "<h1>403 Forbidden</h1>");
		finalizeAndQueue(fd, req, res, false, true);
		return;
	}

	// If it's a directory, refuse
	if (dirExists(fullPath))
	{
		Response res = makeConfigErrorResponse(server, r, 403, "Forbidden", "<h1>403 Forbidden</h1>");
		finalizeAndQueue(fd, req, res, false, true);
		return;
	}

	if (!fileExists(fullPath))
	{
		Response res = makeConfigErrorResponse(server, r, 404, "Not Found", "<h1>404 Not Found</h1>");
		finalizeAndQueue(fd, req, res, false, true);
		return;
	}

	// Attempt remove
	if (std::remove(fullPath.c_str()) == 0)
	{
		Response res;
		res.status_code = 204;
		res.status_message = "No Content";
		res.headers["Content-Length"] = "0";
		res.close_connection = false;
		finalizeAndQueue(fd, req, res, false, true);
		return;
	}

	// unlink failed
	int err = errno;
	if (err == EACCES || err == EPERM)
	{
			Response res = makeConfigErrorResponse(server, r, 403, "Forbidden", "<h1>403 Forbidden</h1>");
			finalizeAndQueue(fd, req, res, false, true);
			return;
	}

	// Generic server error
	Response res = makeConfigErrorResponse(server, r, 500, "Internal Server Error", "<h1>500 Internal Server Error</h1>");
	finalizeAndQueue(fd, req, res, false, true);
}
