#include "SocketManager.hpp"
#include "Config.hpp"
#include "request_reponse_struct.hpp"
#include "utils.hpp"
#include "file_utils.hpp"
#include "Chunked.hpp"
#include <sstream>
#include <iostream>
#include <fstream>

static std::string joinPath(const std::string &a, const std::string &b)
{
	if (a.empty())
		return b;
	if (b.empty())
		return a;

	if (a[a.size() - 1] == '/' )
	{
		if ( b[0] == '/')
			return a + b.substr(1);
		return a + b;
	}
	else
	{
		if (b[0] == '/')
			return a + b;
		return a + "/" + b;
	}
}

//create a name, and avoid collision (using timestamp from epoch)
static std::string makeUploadFileName(const std::string &hint)
{
	static unsigned long s_counter = 0;
	++s_counter;

	// avoid slash from hint

	std::string base = hint;
	size_t pos = base.rfind('/');
	if (pos != std::string::npos)
		base = base.substr(pos +1);
	if (base.empty())
		base = "upload";
	
	//strip dangereous chars (basically we only take AZ_az-09.
	for (size_t i = 0; i < base.size(); ++i)
	{
		char &c = base[i];
		if (!(c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '_' || c == '-' || c == '.'
		)
		{
			c = '_';
		}
	}

	std::ostringstream oss;
	oss << base << (unsigned long)time(NULL) << "_" << s_counter;
	return oss.str();
}


/*
	1) prefer upload_path when present
		build safe absolute path in upload dir
	2) Else: CGI hook (if your config indicates CGI for this route/path)
    If you already have a CGI runner, call it here with `body` as stdin.
    // Example sketch:
    // if (route && route->cgi_enabled) {
    //     runCgi(fd, req, server, *route, body); // must call finalizeAndQueue inside
    //     return;
    // }
*/

void SocketManager::handlePostUploadOrCgi(int fd, 
									const Request &req,
									const ServerConfig &server,
									const RouteConfig *route,
									const std::string &body)
{
	    (void)server; // unused for now; kept for future CGI/env or policy checks 
	if (route && !route->upload_path.empty())
	{
		const std::string &dir = route->upload_path;
		//dir must exist
		if (!dirExists(dir))
		{
			Response res = makeHtmlError(409, "Conflict",  "<h1>409 Conflict</h1><p>Upload directory missing.</p>");
			finalizeAndQueue(fd, req, res, /*body_expected=*/false, /*body_fully_consumed=*/true);
			return;
		}
		//building a safe absolute path within upload dir
		std::string fname = makeUploadFileName(req.path);
		std::string full = joinPath(dir, fname);
		if (!isPathSafe(dir,full))
		{
			Response res = makeHtmlError(403, "Forbidden", "<h1>403 Forbidden</h1>");
			finalizeAndQueue(fd, req, res, false, true);
			return ;
		}
		// writefile
		std::ofstream ofs(full.c_str(), std::ios::out | std::ios:: binary | std::ios::trunc);
		if (!ofs)
		{
			Response res = makeHtmlError(500, "Internal Server Error",
										"<h1>500 Internal Server Error</h1>");
			finalizeAndQueue(fd, req, res, false, true);
			return;
		}
		if (!body.empty())
			ofs.write(&body[0], static_cast<std::streamsize>(body.size()));
		ofs.close();
		Response res;
		res.status_code = 201;
		res.status_message = "Created";
		res.headers["Content-Type"] = "text/plain; charset=utf-8";
		res.body = "Uploaded as: " + fname + "\n";
		res.headers["Content-Length"] = to_string(res.body.size());
		// Optionally expose Location relative to the request route
		// res.headers["Location"] = joinPath(req.path, fname) will see if needed;
		finalizeAndQueue(fd, req, res, false, true);
		return;
	}
	// 2) Else: CGI hook (if your config indicates CGI for this route/path)
	// If you already have a CGI runner, call it here with `body` as stdin.
	// Example sketch:
	// if (route && route->cgi_enabled) {
	//     runCgi(fd, req, server, *route, body); // must call finalizeAndQueue inside
	//     return;
	// }

	// 3) Nothing to do for POST on this route
	{
		Response res = makeHtmlError(404, "Not Found",
									"<h1>404 Not Found</h1><p>No upload or CGI here.</p>");
		finalizeAndQueue(fd, req, res, false, true);
	}
}