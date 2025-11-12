#ifndef REQUEST_REPONSE_STRUCT_HPP
#define REQUEST_REPONSE_STRUCT_HPP

#include <map>
#include <string>

struct Request {
    std::string method;
    std::string path;
    std::string http_version;
    std::map<std::string, std::string> headers;
    std::string body;
    std::map<std::string, std::string> form_fields;
};

struct Response {
    int status_code;                      // e.g., 200, 404, 500
    std::string status_message;           // e.g., "OK", "Not Found", "Internal Server Error"
    std::map<std::string, std::string> headers;  // e.g., Content-Type, Content-Length
    std::string body;                     // The actual body content (HTML, file data, CGI output)
    bool close_connection;                // Whether to close connection after response (Connection: close)
};

std::string build_http_response(const Response &res);

#endif
