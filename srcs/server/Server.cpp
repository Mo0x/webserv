
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
