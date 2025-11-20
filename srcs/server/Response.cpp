#include <sstream>

#include "request_reponse_struct.hpp"

std::string build_http_response(const Response& res)
{
	std::stringstream response;

	// Status line
	response << "HTTP/1.1 " << res.status_code << " " << res.status_message << "\r\n";

	// Headers
	for (std::map<std::string, std::string>::const_iterator it = res.headers.begin();
			it != res.headers.end(); ++it) {
		response << it->first << ": " << it->second << "\r\n";
	}

	// Connection close (if needed)
	if (res.close_connection) {
		response << "Connection: close\r\n";
	}

	// Empty line between headers and body
	response << "\r\n";

	// Body
	response << res.body;

	return response.str();
}
