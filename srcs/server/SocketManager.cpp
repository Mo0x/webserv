/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:34 by mgovinda          #+#    #+#             */
/*   Updated: 2025/08/29 20:30:14 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "SocketManager.hpp"
#include "request_parser.hpp"
#include "request_reponse_struct.hpp"
#include "file_utils.hpp"
#include "utils.hpp"
#include <iostream>
#include <unistd.h>
#include <stdexcept>  // std::runtime_error
#include <string>     // include <cstdiocket.hccept
#include <sys/socket.h> //accept()
#include <cstdio>
#include <fcntl.h> 
#include <sstream> // for std::ostringstream
#include <cstdlib>

/*
	1. getaddrinfo();
	2. socket();
	3. bind();
	4. listen();
	5. accept();
	6. send() || recv();
	7. close() || shutdown() close();

*/

SocketManager::SocketManager(const Config &config) :
	m_config(config)
{
	return ;
}

SocketManager::SocketManager()
{
	return ;
}

SocketManager::SocketManager(const SocketManager &src)
    : m_pollfds(src.m_pollfds),
      m_serverFds(src.m_serverFds),
      m_clientBuffers(src.m_clientBuffers)
{
    // Do NOT copy m_servers — ServerSocket is non-copyable
}

SocketManager &SocketManager::operator=(const SocketManager &src)
{
    if (this != &src)
    {
        m_pollfds = src.m_pollfds;
        m_serverFds = src.m_serverFds;
        m_clientBuffers = src.m_clientBuffers;
        // Do NOT copy m_servers
    }
    return *this;
}

SocketManager::~SocketManager()
{
    for (size_t i = 0; i < m_servers.size(); ++i)
        delete m_servers[i];
}

void SocketManager::addServer(const std::string &host, unsigned short port)
{
	ServerSocket* server = new ServerSocket(host, port);
    m_servers.push_back(server);
    m_serverFds.insert(server->getFd());
}

void SocketManager::initPoll()
{
	m_pollfds.clear();
	for (size_t i = 0; i < m_servers.size(); ++i)
	{
		struct pollfd pfd;
		pfd.fd = m_servers[i]->getFd();
		pfd.events = POLLIN;
		pfd.revents = 0;
		m_pollfds.push_back(pfd);
	}
}

void SocketManager::run()
{
	initPoll();
	while (true)
	{
		int activity = poll(m_pollfds.data(), m_pollfds.size(), -1);
		if (activity < 0 )
		{
			std::cerr << "Poll error" << std::endl;
			continue ;
		}
		for (size_t i = 0; i < m_pollfds.size(); ++i)
		{
			int fd = m_pollfds[i].fd;
			short revents = m_pollfds[i].revents;
			if (revents & POLLIN)
			{
				if (isListeningSocket(fd))
					handleNewConnection(fd);
				else
					handleClientRead(fd);
			}
			else if (revents & POLLOUT)
				handleClientWrite(fd);
			else if (revents & (POLLERR | POLLHUP | POLLNVAL))
				handleClientDisconnect(fd);
		}
	}
}

bool SocketManager::isListeningSocket(int fd) const
{
	return m_serverFds.count(fd) > 0;
}

void SocketManager::handleNewConnection(int listen_fd)
{
	int client_fd = accept(listen_fd, NULL, NULL);
	if (client_fd < 0)
	{
		perror("accept() failed");
		return ;
	}
	std::cout << "Accepted new client: fd " << client_fd << std::endl;
	if(fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0)
	{
		perror("fcntl(F_SETFL) failed");
		close (client_fd);
		return ;
	} 
	struct pollfd pdf;
	pdf.fd = client_fd;
	pdf.events = POLLIN;
	pdf.revents = 0;
	m_pollfds.push_back(pdf);

	m_clientBuffers[client_fd] = "";
	m_headersDone[client_fd] = false;
	m_expectedContentLen[client_fd] = 0;
	m_allowedMaxBody[client_fd] = 0;
	m_isChunked[client_fd] = false;
	for (size_t i = 0; i < m_servers.size(); ++i)
	{
		if (m_servers[i]->getFd() == listen_fd)
		{
			m_clientToServerIndex[client_fd] = i;
			break;
		}
	}
}

void SocketManager::setPollToWrite(int fd)
{
	for (size_t i = 0; i < m_pollfds.size(); ++i)
	{
		if (m_pollfds[i].fd == fd)
		{
			m_pollfds[i].events = POLLOUT;
			break;
		}
	}
}

void SocketManager::handleClientRead(int fd)
{
	char buffer[1024];
	int bytes = recv(fd, buffer, sizeof(buffer), 0);
	if (bytes <= 0)
	{
		handleClientDisconnect(fd);
		return;
	}
	m_clientBuffers[fd].append(buffer, bytes);
	std::cerr << "[FD " << fd << "] recv=" << bytes
          << " bufsize=" << m_clientBuffers[fd].size() << "\n"; //debug to delete
	// DEBUG: after recv
    std::cerr << "[FD " << fd << "] recv=" << bytes
              << " bufsize=" << m_clientBuffers[fd].size() << "\n";

	// Cap header size before we even find the end of headers	
	const size_t HEADER_CAP = 32 * 1024; // 32kb

	// max_body_size implementation 
	// case : headers not finished yet -> enforce cap
	size_t hdrEnd = m_clientBuffers[fd].find("\r\n\r\n");;
	if (hdrEnd == std::string::npos)
	{
		if (m_clientBuffers[fd].size() > HEADER_CAP)
		{
			const ServerConfig &server = m_serversConfig[m_clientToServerIndex[fd]];
			std::string response = buildErrorResponse(431, server);
			m_clientWriteBuffers[fd] = response;
			setPollToWrite(fd);
			std::cerr << "[FD " << fd << "] -> RESP 431 header_cap (no HDR yet)\n";
			return; 
		}
		return ;
	}

	// case :headers are finished -> enforce cap on header block
	if (hdrEnd + 4 > HEADER_CAP)
	{
		const ServerConfig &server = m_serversConfig[m_clientToServerIndex[fd]];
		std::string response = buildErrorResponse(431, server);
		m_clientWriteBuffers[fd] = response;
		setPollToWrite(fd);
		std::cerr << "[FD " << fd << "] -> RESP 431 header_cap (HDR block too big)\n";
		return ;
	}
	// DEBUG: header boundary
    std::cerr << "[FD " << fd << "] hdrEnd=" << (int)hdrEnd
              << " headerBlockLen=" << (int)(hdrEnd+4) << "\n";

	if (!m_headersDone[fd])
	{
		m_headersDone[fd] = true;

		Request req = parseRequest(m_clientBuffers[fd]);

		 std::cerr << "[FD " << fd << "] headersDone method=" << req.method
                  << " path=" << req.path << "\n";
		  
		const ServerConfig& server = m_serversConfig[m_clientToServerIndex[fd]];
		const RouteConfig* route = findMatchingLocation(server, req.path);
		
		size_t allowed = 0;
		if (route && route->max_body_size > 0)
			allowed = route->max_body_size;
		else if (server.client_max_body_size > 0)
			allowed = server.client_max_body_size;
		
		m_allowedMaxBody[fd] = allowed;
		// DEBUG: resolved body limit
        std::cerr << "[FD " << fd << "] allowedMaxBody=" << m_allowedMaxBody[fd] << "\n";


		//Extract content length if its here
		std::map<std::string, std::string>::const_iterator itCL = req.headers.find("Content-Length");
		std::map<std::string, std::string>::const_iterator itTE = req.headers.find("Transfer-Encoding");
        // DEBUG: show CL/TE early
        std::cerr << "[FD " << fd << "] CL=" << (itCL == req.headers.end() ? "<none>" : itCL->second)
                  << " TE=" << (itTE == req.headers.end() ? "<none>" : itTE->second) << "\n";
		if (itCL != req.headers.end())
		{
			//Safe Guard against malformed content-length (ie NaN, overflow etc...)
			bool allDigits = !itCL->second.empty() && itCL->second.find_first_not_of("0123456789") == std::string::npos;
			if (!allDigits)
			{
				std::string response = buildErrorResponse(400, server); //bad request
				m_clientWriteBuffers[fd] = response;
				setPollToWrite(fd);
				std::cerr << "[FD " << fd << "] -> RESP 400 malformed Content-Length\n";
				return ;
			}
			unsigned long cl_ul = strtoul(itCL->second.c_str(), NULL, 10);
			size_t contentLen = static_cast<size_t>(cl_ul);
			m_expectedContentLen[fd] = contentLen;
			//Here early reject if contentLength > limit
			if (allowed > 0 && contentLen > allowed)
			{
				std::string response = buildErrorResponse(413, server);
				m_clientWriteBuffers[fd] = response;
				setPollToWrite(fd);
				std::cerr << "[FD " << fd << "] -> RESP 413 early (CL > allowed)\n";
				return ;
			}

		}
		else
		{
			m_expectedContentLen[fd] = 0;
		}
		// If method expects a body (post/put) but no CL and nor chunker ---> 411 length requiered
		if ((req.method == "POST") || req.method == "PUT")
		{
			bool isChunked = false;
			std::map<std::string, std::string>::const_iterator itTE = req.headers.find("Transfer-Encoding");
			if (itTE != req.headers.end())
			{
				// basic check, full check woud handle comma/params
				std::string te = itTE->second;
				for (size_t i = 0; i < te.size(); ++i)
				te[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(te[i])));
				isChunked = (te.find("chunked") != std::string::npos);
			}
			m_isChunked[fd] = isChunked;
			std::cerr << "[FD " << fd << "] TE chunked?" << (isChunked ? "yes" : "no") << "\n";
			if (!isChunked && itCL == req.headers.end())
			{
				Response res;
				res.status_code = 411;
				res.status_message = "Length Required";
				res.close_connection = true;
				res.headers["Content-Type"] = "text/html";
				res.body = "<h1> 411 Length Required</h1>";
				res.headers["Content-Length"] = to_string(res.body.length());
				m_clientWriteBuffers[fd] = build_http_response(res);
				setPollToWrite(fd);
				std::cerr << "[FD " << fd << "] -> RESP 411 no_length (POST/PUT w/o CL/TE)\n";
				return ;
			}

		}
	}
	/* enforce streaming cap so no CL, chunked or lying clients (ie said body is 20 but is > 20)*/

	size_t hdrEndNow = m_clientBuffers[fd].find("\r\n\r\n");
	size_t bodyStart = (hdrEndNow == std::string::npos) ? 0 : hdrEndNow + 4;
	size_t bodyBytes = (m_clientBuffers[fd].size() > bodyStart) ? (m_clientBuffers[fd].size() - bodyStart) : 0;
	// DEBUG: body counters
    std::cerr << "[FD " << fd << "] bodyStart=" << bodyStart
              << " bodyBytes=" << bodyBytes << "\n";
 

	size_t allowed = m_allowedMaxBody[fd];
	if (allowed > 0 && bodyBytes > allowed)
	{
		const ServerConfig &server = m_serversConfig[m_clientToServerIndex[fd]];
		std::string response = buildErrorResponse(413, server);
		m_clientWriteBuffers[fd] = response;
		setPollToWrite(fd);
		std::cerr << "[FD " << fd << "] -> RESP 413 streaming (body > allowed)\n";
		return ;
	}
	 // OPTIONAL but recommended: client sent more than declared CL → malformed
    if (m_expectedContentLen[fd] > 0 && bodyBytes > m_expectedContentLen[fd]) {
        const ServerConfig &server = m_serversConfig[m_clientToServerIndex[fd]];
        std::string response = buildErrorResponse(400, server);
        m_clientWriteBuffers[fd] = response;
        setPollToWrite(fd);
        std::cerr << "[FD " << fd << "] -> RESP 400 body_overrun (body > CL)\n";
        return;
    }
	// if Content-length is known, wait until we have the full body
	if (m_expectedContentLen[fd] > 0 && bodyBytes < m_expectedContentLen[fd])
	{
		//keep reading;
		std::cerr << "[FD " << fd << "] waiting body... (" << bodyBytes << "/" << m_expectedContentLen[fd] << ")\n";
		return;
	}

	if (m_isChunked[fd])
	{
		 // We don't unchunk yet (Dev B will), but we must not proceed
		// until the chunked body is complete OR we hit the streaming cap.
		// Simple completion check: look for the terminating chunk sequence.
		// NOTE: this ignores optional trailers, which is fine for now.
		size_t endMarkerPos = std::string::npos;
		if (m_clientBuffers[fd].size() >= bodyStart + 5)
		{
			// search for "\r\n0\r\n\r\n" from the body start
			endMarkerPos = m_clientBuffers[fd].find("\r\n0\r\n\r\n", bodyStart);
		}
		if (endMarkerPos == std::string::npos)
		{
			// Not done receiving chunked body yet ->>>> keep reading
			std::cerr << "[FD " << fd << "] waiting chunked body… bodyBytes=" << bodyBytes << "\n";
			return ;
		}
		std::cerr << "[FD " << fd << "] chunked body complete (terminator found)\n";
	}

	Request req = parseRequest(m_clientBuffers[fd]);  // re-parse now that body is ready
	const ServerConfig& server = m_serversConfig[m_clientToServerIndex[fd]];
	const RouteConfig* route   = findMatchingLocation(server, req.path);

	std::string effectiveRoot = (route && !route->root.empty()) ? route->root : server.root;
	std::string effectiveIndex = (route && !route->index.empty()) ? route->index : server.index;

	// Check if method is allowed
	if (route && !route->allowed_methods.empty() &&
		route->allowed_methods.find(req.method) == route->allowed_methods.end())
	{
		std::string response = buildErrorResponse(405, server); // Method Not Allowed
		m_clientWriteBuffers[fd] = response;
		setPollToWrite(fd);
		return;
	}

	// === URI REWRITE ===
	std::string strippedPath = req.path;

	if (route && strippedPath.find(route->path) == 0)
		strippedPath = strippedPath.substr(route->path.length());

	std::cout << "[DEBUG] route->path: " << (route ? route->path : "NULL") << std::endl;
	std::cout << "[DEBUG] strippedPath: " << strippedPath << std::endl;

	// Ensure leading slash
	if (!strippedPath.empty() && strippedPath[0] != '/')
		strippedPath = "/" + strippedPath;

	std::string fullPath = effectiveRoot + strippedPath;
	std::cout << "[DEBUG] fullPath: " << fullPath << std::endl;
	
	if (dirExists(fullPath))
	{
		/*TRAILLING SLASH handling*/
		if (!req.path.empty() && req.path[req.path.length() - 1] != '/')
		{
			Response res;
			res.status_code = 301;
			res.status_message = "Moved Permanently";
			res.headers["Location"] = req.path + "/";
			res.headers["Content-Length"] = "0";
			res.close_connection = true;
			std::string redirectResponse = build_http_response(res);
			m_clientWriteBuffers[fd] = redirectResponse;
			setPollToWrite(fd);
			return ;
		}
		std::string indexCandidate = fullPath + "/" + effectiveIndex;
		std::cout << "[DEBUG] Trying index candidate: " << indexCandidate << std::endl;
		if (fileExists(indexCandidate))
		{
			std::string body = readFile(indexCandidate);
			Response res;
			res.status_code = 200;
			res.status_message = "OK";
			res.body = body;
			res.headers["Content-Type"] = "text/html";
			res.headers["Content-Length"] = to_string(body.length());
			res.close_connection = true;
			std::string response = build_http_response(res);
			m_clientWriteBuffers[fd] = response;
			setPollToWrite(fd);
			std::cerr << "[FD " << fd << "] -> RESP 200 index\n";
			return ;
		}
		if (route && route->autoindex)
		{
			std::string html = generateAutoIndexPage(fullPath, req.path);
			Response res;
			res.status_code = 200;
			res.status_message = "OK";
			res.body = html;
			res.headers["Content-Type"] = "text/html";
			res.headers["Content-Length"] = to_string(html.length());
			res.close_connection = true;
			std::string response = build_http_response(res);
			m_clientWriteBuffers[fd] = response;
			setPollToWrite(fd);
			return;
		}
	}
	/*fall back to index.html if you ask something like localhost:8080/images/ ----> check if index.html exists, if so serve it*/

	std::cout << "[DEBUG] effectiveIndex: " << effectiveIndex << std::endl;
	std::string response;

	if (!isPathSafe(effectiveRoot, fullPath)) {
		response = buildErrorResponse(403, server);
		std::cerr << "[FD " << fd << "] -> RESP 403 unsafe_path\n";
	}
	else if (!fileExists(fullPath)) {
		response = buildErrorResponse(404, server);
		std::cerr << "[FD " << fd << "] -> RESP 404 not_found\n";
	}
	else {
		std::string body = readFile(fullPath);
		Response res;
		res.status_code = 200;
		res.status_message = "OK";
		res.body = body;
		res.headers["Content-Type"] = "text/html";
		res.headers["Content-Length"] = to_string(body.length());
		res.close_connection = true;
		response = build_http_response(res);
		std::cerr << "[FD " << fd << "] -> RESP 200 file\n";
	}

	m_clientWriteBuffers[fd] = response;
	setPollToWrite(fd);
}

//previous handleClientRead() without rooting logic

/* void SocketManager::handleClientRead(int fd)
{
	char buffer[1024];
	int bytes = recv(fd, buffer, sizeof(buffer), 0);
	if (bytes <= 0)
	{
		handleClientDisconnect(fd);
		return ;
	}
	m_clientBuffers[fd].append(buffer, bytes);

	// Wait for full HTTP request (simplified, assumes no body)
	if (m_clientBuffers[fd].find("\r\n\r\n") == std::string::npos)
		return ;

	Request req = parseRequest(m_clientBuffers[fd]);
	const ServerConfig& server = m_serversConfig[m_clientToServerIndex[fd]]; 
	std::string filePath = (req.path == "/") ? "/index.html" : req.path;
	std::string basePath = "./www";
	std::string fullPath = basePath + filePath;

	std::string response;

	if (!isPathSafe(basePath, fullPath)) {
		response = buildErrorResponse(403, server);
	}
	else if (!fileExists(fullPath)) {
		response = buildErrorResponse(404, server);
	}
	else {
		std::string body = readFile(fullPath);
		Response res;
		res.status_code = 200;
		res.status_message = "OK";
		res.body = body;
		res.headers["Content-Type"] = "text/html";
		res.headers["Content-Length"] = to_string(body.length());
		res.close_connection = true;
		response = build_http_response(res);
	}

	m_clientWriteBuffers[fd] = response;

	// Update pollfd to POLLOUT so we can send the response
	for (size_t i = 0; i < m_pollfds.size(); ++i)
	{
		if (m_pollfds[i].fd == fd)
		{
			m_pollfds[i].events = POLLOUT;
			break;
		}
	}
} */

void SocketManager::handleClientWrite(int fd)
{
	if (m_clientWriteBuffers.find(fd) == m_clientWriteBuffers.end())
		return ;
	std::string &buffer = m_clientWriteBuffers[fd];
	ssize_t sent = send(fd, buffer.c_str(), buffer.size(), 0);
	if (sent < 0)
	{
		perror("send() failed");
		handleClientDisconnect(fd);
		return ;
	}

	buffer.erase(0,sent);
	if (buffer.empty())
	{
		m_clientWriteBuffers.erase(fd);
		handleClientDisconnect(fd);
	}
}

void SocketManager::handleClientDisconnect(int fd)
{
	std::cout << "Disconnecting fd " << fd << std::endl;
	close(fd);
	m_clientBuffers.erase(fd);
	m_clientWriteBuffers.erase(fd);
	m_clientToServerIndex.erase(fd);
	for (size_t i = 0; i < m_pollfds.size(); ++i)
	{
		if (m_pollfds[i].fd == fd)
		{
			m_pollfds.erase(m_pollfds.begin() + i);
			break ; 
		}
	}
	m_headersDone.erase(fd);
	m_expectedContentLen.erase(fd);
	m_allowedMaxBody.erase(fd);
	m_isChunked.erase(fd);
}

static std::string getStatusMessage(int code)
{
	switch (code)
	{
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 411: return "Length Required";
		case 413: return "Payload Too Large";
		case 431: return "Request Header Fields Too Large";
		case 500: return "Internal Server Error";
		default:  return "Error";
	}
}

std::string SocketManager::buildErrorResponse(int code, const ServerConfig &server)
{
	Response res;
	res.status_code = code;
	res.close_connection = true;
	res.headers["Content-Type"] = "text/html";

	std::map<int, std::string>::const_iterator it = server.error_pages.find(code);
	if (it != server.error_pages.end())
	{
		std::string relativePath = it->second;
		if (!relativePath.empty() && relativePath[0] == '/')
			relativePath = relativePath.substr(1);

		std::string fullPath = server.root + "/" + relativePath;
		std::cout << "[DEBUG] server.root = " << server.root << std::endl;
		std::cout << "[DEBUG] Looking for custom error page: " << fullPath << std::endl;

		if (fileExists(fullPath))
		{
			res.body = readFile(fullPath);
			res.status_message = getStatusMessage(code);
			res.headers["Content-Length"] = to_string(res.body.length());
			return build_http_response(res);
		}
	}

	// fallback default body
	res.status_message = getStatusMessage(code);
	res.body = "<h1>" + to_string(code) + " " + res.status_message + "</h1>";
	res.headers["Content-Length"] = to_string(res.body.length());
	return build_http_response(res);
}

void SocketManager::setServers(const std::vector<ServerConfig> & servers)
{
	m_serversConfig = servers;
}

const ServerConfig& SocketManager::findServerForClient(int fd) const
{
	std::map<int, size_t>::const_iterator it = m_clientToServerIndex.find(fd);
	if (it == m_clientToServerIndex.end())
		return m_config.servers[it->second];
	throw std::runtime_error("No matching server config for client FD");
}


//previous run funciton now its cleaner and divided in multiple functions.

/* void SocketManager::run()
{
	initPoll();
	std::cout << "Starting poll() loop on " << m_pollfds.size() << " sockets." << std::endl;

	while (true)
	{
		std::cout << "[DEBUG] Polling..." << std::endl;
		int ret = poll(&m_pollfds[0], m_pollfds.size(), -1);
		std::cout << "[DEBUG] poll() returned: " << ret << std::endl;

		if (ret < 0)
		{
			perror("poll() failed");
			break;
		}

		for (size_t i = 0; i < m_pollfds.size(); ++i)
		{
			int fd = m_pollfds[i].fd;

			std::cout << "[DEBUG] fd: " << fd
			          << " events: " << m_pollfds[i].events
			          << " revents: " << m_pollfds[i].revents << std::endl;

			if (m_pollfds[i].revents & POLLIN)
			{
				if (m_serverFds.count(fd))
				{
					int client_fd = accept(fd, NULL, NULL);
					if (client_fd >= 0)
					{
						std::cout << "Accepted new client: fd " << client_fd << std::endl;
					
						if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0)
						{
							perror("fcntl(F_SETFL) failed");
							close(client_fd);
							continue;
						}
					
						struct pollfd client_poll;
						client_poll.fd = client_fd;
						client_poll.events = POLLIN;
						client_poll.revents = 0;
						m_pollfds.push_back(client_poll);
					
						m_clientBuffers[client_fd] = "";
						std::cout << "[DEBUG] Client fd " << client_fd << " added to poll()" << std::endl;
					}
					else
					{
						perror("accept() failed");
					}
				}
				else
				{
					char buffer[1024];
					int bytes = recv(fd, buffer, sizeof(buffer), 0);

					if (bytes <= 0)
					{
						std::cout << "Client disconnected: fd " << fd << std::endl;
						close(fd);
						m_clientBuffers.erase(fd);
						m_pollfds.erase(m_pollfds.begin() + i);
						--i;
					}
					else
					{
						m_clientBuffers[fd].append(buffer, bytes);

						if (m_clientBuffers[fd].find("\r\n\r\n") != std::string::npos)
						{
							Request req = parseRequest(m_clientBuffers[fd]);

							std::cout << "[PARSED] Method: " << req.method << "\n";
							std::cout << "[PARSED] Path: " << req.path << "\n";
							std::cout << "[PARSED] Version: " << req.http_version << "\n";

							for (std::map<std::string, std::string>::iterator it = req.headers.begin();
								it != req.headers.end(); ++it)
								std::cout << "[HEADER] " << it->first << ": " << it->second << "\n";

							std::string basePath = "./www";
							std::string filePath = req.path;
							if (filePath == "/")
								filePath = "/index.html";
							std::string fullPath = basePath + filePath;
							if (!isPathSafe(basePath, fullPath))
							{
								std::string body = "<h1>403 Forbidden</h1>";
								std::ostringstream oss;
								oss << "HTTP/1.1 403 Forbidden\r\n";
								oss << "Content-Length: " << body.size() << "\r\n";
								oss << "Content-Type: text/html\r\n\r\n";
								oss << body;
								std::string response = oss.str();
								send(fd, response.c_str(), response.size(), 0);
								close(fd);
								m_clientBuffers.erase(fd);
								m_pollfds.erase(m_pollfds.begin() + i);
								--i;
								continue;
							}
							std::string response;
							if (file_utils::exists(fullPath))
							{
								std::string body = file_utils::readFile(fullPath);								std::ostringstream oss;
								oss << "HTTP/1.1 200 OK\r\n";
								oss << "Content-Length: " << body.size() << "\r\n";
								oss << "Content-Type: text/html\r\n"; // TODO: detect MIME type
								oss << "\r\n";
								oss << body;
								response = oss.str();
							}
							else
							{
								std::string body = "<h1>404 Not Found</h1>";
								std::ostringstream oss;
								oss << "HTTP/1.1 404 Not Found\r\n";
								oss << "Content-Length: " << body.size() << "\r\n";
								oss << "Content-Type: text/html\r\n";
								oss << "\r\n";
								oss << body;
								response = oss.str();
							}

							send(fd, response.c_str(), response.size(), 0);

							close(fd);
							m_clientBuffers.erase(fd);
							m_pollfds.erase(m_pollfds.begin() + i);
							--i;
						}
						else
						{
							std::cout << "Received partial data from fd " << fd << ": " << m_clientBuffers[fd] << std::endl;
						}
					}
				}
			}
		}
	}
}
 */