
#include "Server.hpp"
#include "ServerConfig.hpp"
#include "SocketManager.hpp"
#include "RequestParser.hpp"
#include "request_response_struct.hpp"
#include "Logger.hpp"
#include "file_utils.hpp"
#include "utils.hpp"
#include <sys/stat.h>
#include <string>
#include <dirent.h>
#include <vector>
#include <algorithm>
#include <sstream>

// Constructor: store the config and initialize your socket manager
Server::Server(const ServerConfig& config)
    : _config(config),
      // SocketManager takes host C-string and port int
      _sockMgr(config.getHost().c_str(), config.getPort())
{ }

Server::~Server()
{ }

// Main accept-and-dispatch loop
void Server::run()
{
    // Start listening
    if (!_sockMgr.listen())
    {
        Logger::error("Failed to listen on "
                      + _config.getHost()
                      + ":" + static_cast<std::ostringstream&>((std::ostringstream() << _config.getPort())).str());
        return;
    }
    Logger::info("Server listening on "
                 + _config.getHost()
                 + ":" + static_cast<std::ostringstream&>((std::ostringstream() << _config.getPort())).str());

    while (true)
    {
        int clientFd = _sockMgr.accept();
        if (clientFd < 0)
        {
            Logger::warn("Accept failed, retrying");
            continue;
        }

        // 1) Read & parse incoming HTTP request
        std::string rawReq = _sockMgr.recv(clientFd);
        Request     req    = RequestParser::parse(rawReq);

        // 2) Find best‐matching location block (or nullptr)
        const LocationConfig* loc =
            _config.findBestLocation(req.path);
        if (loc == NULL)
        {
            // No route → 404 Not Found
            Response res404;
            res404.setStatus(404);
            res404.setBody("<h1>404 Not Found</h1>");
            _sockMgr.send(clientFd, res404.toString());
            _sockMgr.close(clientFd);
            continue;
        }

        // 3) For now, just echo back which prefix matched
        bool method_ok = false;
        for (size_t i = 0; i < loc->allowedMethods.size(); ++i) {
            if (loc->allowedMethods[i] == req.method) {
                method_ok = true;
                break;
            }
        }
        if (!method_ok) {
            Response res;
            res.setStatus(405);
            // build “Allow: GET, POST” header
            std::string allow = "";
            for (size_t i = 0; i < loc->allowedMethods.size(); ++i) {
                if (i) allow += ", ";
                allow += loc->allowedMethods[i];
            }
            res.addHeader("Allow", allow);
            res.setBody("<h1>405 Method Not Allowed</h1>");
            _sockMgr.send(clientFd, res.toString());
            _sockMgr.close(clientFd);
            continue;
        }
        // 3) Build candidate file path
        std::string prefix = loc->path;              // e.g. "/images"
        std::string rest   = req.path.substr(prefix.size());
        std::string rawPath = loc->root + rest;      // e.g. "/var/www/html" + "/foo.png"

        // 4) Normalize & guard against “../”
        std::string fullPath = file_utils::normalize(rawPath);
        if (fullPath.find(loc->root) != 0) {
            // tried to break out of root
            Response res(403, "<h1>403 Forbidden</h1>");
            _sockMgr.send(clientFd, res.toString());
            _sockMgr.close(clientFd);
            continue;
        }

    // 5) If directory, try index or autoindex
    struct stat st;
    if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        // directory
        if (!loc->index.empty()) {
            fullPath += "/" + loc->index;
        } else if (loc->autoindex) {
            // generate directory listing using shared helper and return as HTML
            std::string body = generateAutoIndexPage(fullPath, req.path);
            Response res200;
            res200.setStatus(200);
            res200.addHeader("Content-Type", "text/html; charset=utf-8");
            res200.setBody(body);
            _sockMgr.send(clientFd, res200.toString());
            _sockMgr.close(clientFd);
            continue;
        } else {
            Response res(403, "<h1>403 Forbidden</h1>");
            _sockMgr.send(clientFd, res.toString());
            _sockMgr.close(clientFd);
            continue;
        }
    }

    // 6) Check file exists & is regular
    if (stat(fullPath.c_str(), &st) < 0 || !S_ISREG(st.st_mode)) {
        Response res(404, "<h1>404 Not Found</h1>");
        _sockMgr.send(clientFd, res.toString());
        _sockMgr.close(clientFd);
        continue;
    }

    // 7) Read & serve the file
    std::string body = file_utils::readFile(fullPath);
    Response res200;
    res200.setStatus(200);
    res200.addHeader("Content-Type", file_utils::getMimeType(fullPath));
    res200.setBody(body);
    _sockMgr.send(clientFd, res200.toString());
    _sockMgr.close(clientFd);
    }
}

// Use shared `generateAutoIndexPage` from utils.cpp instead of a duplicate implementation.

// Copy‐ctor
Server::Server(const Server& src)
    : _config(src._config),
      _sockMgr(src._sockMgr)
{ }

// Assignment
Server& Server::operator=(const Server& src)
{
    if (this != &src)
    {
        _config  = src._config;
        _sockMgr = src._sockMgr;
    }
    return *this;
}