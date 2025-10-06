/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:34 by mgovinda          #+#    #+#             */
/*   Updated: 2025/10/06 17:45:52 by mgovinda         ###   ########.fr       */
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
#include <cerrno>
#include <cctype>

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
			m_pollfds[i].events |= POLLOUT;
			break;
		}
	}
}

void SocketManager::clearPollout(int fd)
{
	for (size_t i = 0; i < m_pollfds.size(); ++i)
	{
		if (m_pollfds[i].fd == fd)
		{
			m_pollfds[i].events &= ~POLLOUT; // remove write interest ONLY operator bitwise AND (&=) and NOT (~) to just remove pollout
			break;
		}
	}
}

static Response makeHtmlError(int code, const std::string& reason, const std::string& html)
{

	Response r;
	r.status_code = code;
	r.status_message = reason;
	r.headers["Content-Type"] = "text/html; charset=utf-8";
	r.headers["Content-Length"] = to_string(html.size());
	r.body = html;
	return r;
}

static int validateBodyFraming(const std::map<std::string, std::string> &headers, size_t &outContentLength, bool &outHasTE)
{
	outContentLength = 0;
	outHasTE = false;
	size_t clCount = 0;
	std::string clValue;

	for (std::map<std::string , std::string>::const_iterator it = headers.begin(); it != headers.end(); it++)
	{
		const std::string &key = it->first;
		const std::string &value = it->second;
		if (key == "content-length")
		{
			++clCount;
			clValue = value;
		}
		else if (key == "transfer-encoding")
		{
			outHasTE = true;
		}
	}
	if (clCount > 1)
		return 400;
	if (clCount == 1 && outHasTE)
		return 400;
	// if CL present, verify digits-only and then parse into size_t safely
	if (clCount == 1)
	{
		if (clValue.empty())
			return 400;
		for (size_t i = 0; i < clValue.size(); ++i)
		{
			const unsigned char ch = static_cast<unsigned char>(clValue[i]);
			if (ch < '0' || ch > '9')
				return 400;
		}

		/* here we parse with a guard against overflow !
		Let’s say size_t is 64 bits, so its max is 18446744073709551615.

		MAX_DIV10 = the largest integer X such that X * 10 still fits in size_t.
		→ here it’s 1844674407370955161 (we drop the last digit 5).

		MAX_LAST = the remaining “last safe digit” (the remainder of max ÷ 10)
		→ here it’s 5.

		EG:
		Say size_t max = 999.

		Then MAX_DIV10 = 99 and MAX_LAST = 9.

		Now parsing "1000":

		step	val	digit	check
		'1'		0	1		ok → val=1
		'0'		1	0		ok → val=10
		'0'		10	0		ok → val=100
		'0'		100	0		val(100) > MAX_DIV10(99) → reject 400
		*/
		size_t val = 0;
		const size_t MAX_DIV10 = static_cast<size_t>(-1) / 10;
		const size_t MAX_LAST = static_cast<size_t>(-1) % 10;

		for (size_t i = 0; i < clValue.size(); ++i)
		{
			size_t digit = static_cast<size_t>(clValue[i] - '0'); // classic ascii trick 
			if (val > MAX_DIV10 || (val == MAX_DIV10 && digit > MAX_LAST))
				return 400;
			val = val * 10 + digit;
		}
		outContentLength = val;
	}
	return 0; // means OK here
}

void SocketManager::handleClientRead(int fd)
{
    char buffer[1024];
    int bytes = recv(fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
        handleClientDisconnect(fd);
        return;
    }
    m_clientBuffers[fd].append(buffer, bytes);
    std::cerr << "[FD " << fd << "] recv=" << bytes
              << " bufsize=" << m_clientBuffers[fd].size() << "\n";

    // ---- Header cap checks (pre-parse) --------------------------------------
    const size_t HEADER_CAP = 32 * 1024;
    size_t hdrEnd = m_clientBuffers[fd].find("\r\n\r\n");
    if (hdrEnd == std::string::npos) 
	{
        if (m_clientBuffers[fd].size() > HEADER_CAP) {
            Response res = makeHtmlError(431, "Request Header Fields Too Large",
                                         "<h1>431 Request Header Fields Too Large</h1>");
            res.close_connection = true; // headers not complete → must close
            m_clientWriteBuffers[fd] = build_http_response(res);
            setPollToWrite(fd);
            std::cerr << "[FD " << fd << "] -> RESP 431 header_cap (no HDR yet)\n";
            return;
        }
        return; // keep reading headers
    }
	// ----- Header line count cap (extra guard) --------------------------------

	const size_t MAX_HEADER_LINES = 100;
	size_t lines = 0, pos = 0;
	while (pos < hdrEnd)
	{
		size_t nl = m_clientBuffers[fd].find("\r\n", pos);
		if (nl == std::string::npos || nl > hdrEnd)
			break ;
		if (lines > MAX_HEADER_LINES)
		{
			Response res = makeHtmlError(431, "Request Header Fields Too Large", "<h1>431 Request Header Fields Too Large</h1>");
			res.close_connection = true;
			m_clientWriteBuffers[fd] = build_http_response(res);
			setPollToWrite(fd);
			std::cerr << "[FD " << fd << "] -> RESP 431 header_line_count\n";
			return;
		}
		pos = nl + 2;
	}

    if (hdrEnd + 4 > HEADER_CAP) {
        Response res = makeHtmlError(431, "Request Header Fields Too Large",
                                     "<h1>431 Request Header Fields Too Large</h1>");
        res.close_connection = true; // header block too big → close
        m_clientWriteBuffers[fd] = build_http_response(res);
        setPollToWrite(fd);
        std::cerr << "[FD " << fd << "] -> RESP 431 header_cap (HDR block too big)\n";
        return;
    }

    // ---- Parse ONCE (now that headers are complete & within cap) ------------
    Request req = parseRequest(m_clientBuffers[fd]);

	//here do the guard against HTML request smuggling
	size_t contentLength = 0;
	bool hasTE = false;
	int framingErr = validateBodyFraming(req.headers, contentLength, hasTE);
	if (framingErr != 0)
	{
		Response res = makeHtmlError(400, "Bad Request", "<h1>400 Bad Request</h1><p>Invalid Content-Length / Transfer-Encoding</p>");
		res.close_connection = true;
		m_clientWriteBuffers[fd] = build_http_response(res);
		setPollToWrite(fd);
		return ;
	}
    const std::string methodUpper = toUpperCopy(req.method);

    // ---- First-time header processing (limits, TE/CL, 405/411) --------------
    if (!m_headersDone[fd]) {
        m_headersDone[fd] = true;

        const ServerConfig& server = m_serversConfig[m_clientToServerIndex[fd]];
        const RouteConfig* route   = findMatchingLocation(server, req.path);

        // Resolve allowed body size (route overrides server)
        size_t allowed = 0;
        if (route && route->max_body_size > 0) allowed = route->max_body_size;
        else if (server.client_max_body_size > 0) allowed = server.client_max_body_size;
        m_allowedMaxBody[fd] = allowed;
        std::cerr << "[FD " << fd << "] allowedMaxBody=" << m_allowedMaxBody[fd] << "\n";

        // 405 early (pre-body)
        if (route && !route->allowed_methods.empty() &&
            !isMethodAllowedForRoute(methodUpper, route->allowed_methods)) {
            std::set<std::string> allowSet = normalizeAllowedForAllowHeader(route->allowed_methods);
            std::string allowHeader = joinAllowedMethods(allowSet);
            Response res;
            res.status_code = 405;
            res.status_message = "Method Not Allowed";
            res.headers["Allow"] = allowHeader;
            res.headers["Content-Type"] = "text/html; charset=utf-8";
            res.body = "<h1>405 Method Not Allowed</h1>";
            res.headers["Content-Length"] = to_string(res.body.length());
            const bool body_expected = (methodUpper == "POST" || methodUpper == "PUT");
            const bool body_fully_consumed = false;
            std::cerr << "[FD " << fd << "] -> RESP 405 disallowed_method (pre-body)\n";
            finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
            return;
        }

        // Extract CL/TE
        std::map<std::string, std::string>::const_iterator itCL = req.headers.find("Content-Length");
        std::map<std::string, std::string>::const_iterator itTE = req.headers.find("Transfer-Encoding");

        std::cerr << "[FD " << fd << "] CL=" << (itCL == req.headers.end() ? "<none>" : itCL->second)
                  << " TE=" << (itTE == req.headers.end() ? "<none>" : itTE->second) << "\n";

        if (itCL != req.headers.end()) {
            // Validate CL
            bool allDigits = !itCL->second.empty() &&
                             itCL->second.find_first_not_of("0123456789") == std::string::npos;
            if (!allDigits) {
                Response res = makeHtmlError(400, "Bad Request", "<h1>400 Bad Request</h1>");
                const bool body_expected = (methodUpper == "POST" || methodUpper == "PUT");
                const bool body_fully_consumed = false;
                std::cerr << "[FD " << fd << "] -> RESP 400 malformed Content-Length\n";
                finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
                return;
            }
            unsigned long cl_ul = strtoul(itCL->second.c_str(), NULL, 10);
            size_t contentLen = static_cast<size_t>(cl_ul);
            m_expectedContentLen[fd] = contentLen;

            // Early 413 if CL > limit
            if (m_allowedMaxBody[fd] > 0 && contentLen > m_allowedMaxBody[fd]) {
                Response res = makeHtmlError(413, "Payload Too Large", "<h1>413 Payload Too Large</h1>");
                const bool body_expected = (methodUpper == "POST" || methodUpper == "PUT");
                const bool body_fully_consumed = false;
                std::cerr << "[FD " << fd << "] -> RESP 413 early (CL > allowed)\n";
                finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
                return;
            }
        } else {
            m_expectedContentLen[fd] = 0;
        }

        // TE: chunked?
        bool isChunked = false;
        if (itTE != req.headers.end()) {
            std::string te = itTE->second;
            for (size_t i = 0; i < te.size(); ++i)
                te[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(te[i])));
            isChunked = (te.find("chunked") != std::string::npos);
        }
        m_isChunked[fd] = isChunked;
        std::cerr << "[FD " << fd << "] TE chunked? " << (isChunked ? "yes" : "no") << "\n";

        // 411 if POST/PUT and neither CL nor chunked
        if ((methodUpper == "POST" || methodUpper == "PUT") &&
            !isChunked && itCL == req.headers.end()) {
            Response res;
            res.status_code = 411;
            res.status_message = "Length Required";
            res.headers["Content-Type"] = "text/html; charset=utf-8";
            res.body = "<h1> 411 Length Required</h1>";
            res.headers["Content-Length"] = to_string(res.body.length());
            const bool body_expected = true;
            const bool body_fully_consumed = true; // there is no body to read
            std::cerr << "[FD " << fd << "] -> RESP 411 no_length (POST/PUT w/o CL/TE)\n";
            finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
            return;
        }
    }

    // ---- Body accounting / streaming guards ---------------------------------
    size_t hdrEndNow = m_clientBuffers[fd].find("\r\n\r\n");
    size_t bodyStart = (hdrEndNow == std::string::npos) ? 0 : hdrEndNow + 4;
    size_t bodyBytes = (m_clientBuffers[fd].size() > bodyStart)
                       ? (m_clientBuffers[fd].size() - bodyStart) : 0;

    std::cerr << "[FD " << fd << "] bodyStart=" << bodyStart
              << " bodyBytes=" << bodyBytes << "\n";

    // 413 streaming cap
    if (m_allowedMaxBody[fd] > 0 && bodyBytes > m_allowedMaxBody[fd]) {
        Response res = makeHtmlError(413, "Payload Too Large", "<h1>413 Payload Too Large</h1>");
        const bool body_expected = (methodUpper == "POST" || methodUpper == "PUT") || m_isChunked[fd];
        const bool body_fully_consumed = false;
        std::cerr << "[FD " << fd << "] -> RESP 413 streaming (body > allowed)\n";
        finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
        return;
    }

    // 400 if overrun past CL
    if (m_expectedContentLen[fd] > 0 && bodyBytes > m_expectedContentLen[fd]) {
        Response res = makeHtmlError(400, "Bad Request", "<h1>400 Bad Request</h1>");
        const bool body_expected = (methodUpper == "POST" || methodUpper == "PUT") || m_isChunked[fd];
        const bool body_fully_consumed = false;
        std::cerr << "[FD " << fd << "] -> RESP 400 body_overrun (body > CL)\n";
        finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
        return;
    }

    // Wait for full CL body
    if (m_expectedContentLen[fd] > 0 && bodyBytes < m_expectedContentLen[fd]) {
        std::cerr << "[FD " << fd << "] waiting body... (" << bodyBytes
                  << "/" << m_expectedContentLen[fd] << ")\n";
        return;
    }

    // Wait for full chunked terminator (placeholder until real decoder)
    if (m_isChunked[fd]) {
        size_t endMarkerPos = std::string::npos;
        if (m_clientBuffers[fd].size() >= bodyStart + 5)
            endMarkerPos = m_clientBuffers[fd].find("\r\n0\r\n\r\n", bodyStart);
        if (endMarkerPos == std::string::npos) {
            std::cerr << "[FD " << fd << "] waiting chunked body… bodyBytes=" << bodyBytes << "\n";
            return;
        }
        std::cerr << "[FD " << fd << "] chunked body complete (terminator found)\n";
    }

    // ---- Dispatch (static/autoindex/redirect) --------------------------------
    const ServerConfig& server = m_serversConfig[m_clientToServerIndex[fd]];
    const RouteConfig* route   = findMatchingLocation(server, req.path);

    std::string effectiveRoot  = (route && !route->root.empty())  ? route->root  : server.root;
    std::string effectiveIndex = (route && !route->index.empty()) ? route->index : server.index;

    // Rewrite to stripped path (route prefix removed)
    std::string strippedPath = req.path;
    if (route && strippedPath.find(route->path) == 0)
        strippedPath = strippedPath.substr(route->path.length());
    if (!strippedPath.empty() && strippedPath[0] != '/')
        strippedPath = "/" + strippedPath;

    std::string fullPath = effectiveRoot + strippedPath;

    if (dirExists(fullPath)) {
        // Trailing slash redirect
        if (!req.path.empty() && req.path[req.path.length() - 1] != '/') {
            Response res;
            res.status_code = 301;
            res.status_message = "Moved Permanently";
            res.headers["Location"] = req.path + "/";
            res.headers["Content-Length"] = "0";
            const bool body_expected = false;
            const bool body_fully_consumed = true;
            std::cerr << "[FD " << fd << "] -> RESP 301 redirect add-trailing-slash\n";
            finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
            return;
        }
        // Try index file
        std::string indexCandidate = fullPath;
        if (!indexCandidate.empty() && indexCandidate[indexCandidate.size() - 1] != '/')
            indexCandidate += '/';
        indexCandidate += effectiveIndex;
        if (fileExists(indexCandidate)) {
            std::string body = readFile(indexCandidate);
            Response res;
            res.status_code = 200;
            res.status_message = "OK";
            res.body = body;
            res.headers["Content-Type"] = getMimeTypeFromPath(indexCandidate);
            res.headers["Content-Length"] = to_string(body.length());
            if (methodUpper == "HEAD") res.body.clear();
            const bool body_expected = false;
            const bool body_fully_consumed = true;
            std::cerr << "[FD " << fd << "] -> RESP 200 index\n";
            finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
            return;
        }
        // Autoindex
        if (route && route->autoindex) {
            std::string html = generateAutoIndexPage(fullPath, req.path);
            Response res;
            res.status_code = 200;
            res.status_message = "OK";
            res.body = html;
            res.headers["Content-Type"] = "text/html; charset=utf-8";
            res.headers["Content-Length"] = to_string(html.length());
            if (methodUpper == "HEAD") res.body.clear();
            const bool body_expected = false;
            const bool body_fully_consumed = true;
            std::cerr << "[FD " << fd << "] -> RESP 200 autoindex\n";
            finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
            return;
        }
    }

    // Path safety / existence
    if (!isPathSafe(effectiveRoot, fullPath)) {
        Response res = makeHtmlError(403, "Forbidden", "<h1>403 Forbidden</h1>");
        const bool body_expected = false;
        const bool body_fully_consumed = true;
        std::cerr << "[FD " << fd << "] -> RESP 403 unsafe_path\n";
        finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
        return;
    }

    if (!fileExists(fullPath)) {
        Response res = makeHtmlError(404, "Not Found", "<h1>404 Not Found</h1>");
        const bool body_expected = false;
        const bool body_fully_consumed = true;
        std::cerr << "[FD " << fd << "] -> RESP 404 not_found\n";
        finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
        return;
    }

    // Static file 200
    {
        std::string body = readFile(fullPath);
        Response res;
        res.status_code = 200;
        res.status_message = "OK";
        res.body = body;
        res.headers["Content-Type"] = getMimeTypeFromPath(fullPath);
        res.headers["Content-Length"] = to_string(body.length());
        if (methodUpper == "HEAD") res.body.clear();
        const bool body_expected = false;
        const bool body_fully_consumed = true;
        std::cerr << "[FD " << fd << "] -> RESP 200 file\n";
        finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
        return;
    }
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
	std::map<int, std::string>::iterator it = m_clientWriteBuffers.find(fd);
	if (it == m_clientWriteBuffers.end())
		return;
	std::string &buffer = it->second;
	if (buffer.empty())
	{
		m_clientWriteBuffers.erase(it);
		handleClientDisconnect(fd);
		return;
	}

	const size_t MAX_CHUNK = 16 * 1024;
	const size_t toSend = buffer.size() < MAX_CHUNK ? buffer.size() : MAX_CHUNK;

	ssize_t n = ::send(fd, buffer.data(), toSend, 0); // ::send <---- because we want to use send the system call and not some SocketManager::sent

	if (n < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			return ;
		}
		handleClientDisconnect(fd);
		return ;
	}

	buffer.erase(0, static_cast<size_t>(n));
	if (!buffer.empty())
	{
		// Still have data; keep POLLOUT set (we already changed setPollToWrite to |=).
		return ;
	}
	m_clientWriteBuffers.erase(it);
	handleClientDisconnect(fd);
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
	res.headers["Content-Type"] = "text/html; charset=utf-8";

	std::map<int, std::string>::const_iterator it = server.error_pages.find(code);
	if (it != server.error_pages.end())
	{
		std::string relativePath = it->second;
		if (!relativePath.empty() && relativePath[0] == '/')
			relativePath = relativePath.substr(1);

		std::string fullPath = server.root;
			if (!fullPath.empty() && fullPath[fullPath.size()-1] == '/')
				fullPath.erase(fullPath.size()-1);
		fullPath += "/" + relativePath;

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
		throw std::runtime_error("No matching server config for client FD");
	return m_config.servers[it->second];
}

void SocketManager::finalizeAndQueue(int fd, const Request &req, Response &res, bool body_expected, bool body_fully_consumed)
{
	const bool headers_complete = true; 
	const bool client_close = clientRequestedClose(req);

	const bool close_it = shouldCloseAfterThisResponse(res.status_code, headers_complete, body_expected, body_fully_consumed, client_close);
	res.close_connection = close_it;

	m_clientWriteBuffers[fd] = build_http_response(res);
	setPollToWrite(fd);
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