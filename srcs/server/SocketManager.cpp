/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:34 by mgovinda          #+#    #+#             */
/*   Updated: 2025/11/05 17:32:31 by mgovinda         ###   ########.fr       */
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
/* helper for safeguard*/


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

static void queueErrorAndClose(SocketManager &sm, int fd, int status,
							   const std::string &title,
							   const std::string &html)
{
	Response res = makeHtmlError(status, title, html);
	res.close_connection = true;     // invalid headers → must close
	sm.finalizeAndQueue(fd, res);
}

//new helper of readIntoClientBuffer that takes ClientState
bool SocketManager::readIntoBuffer(int fd, ClientState &st)
{
	char buffer[4096];
	ssize_t bytes = ::recv(fd, buffer, sizeof(buffer), 0);
	
	if (bytes == 0)
		return false;
	
	if (bytes < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return true;  // no new data yet, but keep existing buffered bytes
		return false;
	}
	st.recvBuffer.append(buffer, bytes);
	return true;
}


bool SocketManager::readIntoClientBuffer(int fd)
{
	// ---- read from socket (nonblocking-safe) --------------------------------
	char buffer[1024];
	int bytes = ::recv(fd, buffer, sizeof(buffer), 0);
	if (bytes == 0)
	{
		handleClientDisconnect(fd);
		return false;
	}
	if (bytes < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return true;  // no new data yet, but keep existing buffered bytes
		handleClientDisconnect(fd);
		return false;
	}
	m_clientBuffers[fd].append(buffer, bytes);
	return true;
}

bool SocketManager::locateHeaders(int fd, size_t &hdrEnd)
{
	// ---- header byte cap pre-check (before we have full headers) -----------
	const size_t HEADER_CAP = 32 * 1024;
	if (m_clientBuffers[fd].size() > HEADER_CAP &&
		m_clientBuffers[fd].find("\r\n\r\n") == std::string::npos)
	{
		queueErrorAndClose(*this, fd, 431, "Request Header Fields Too Large",
						   "<h1>431 Request Header Fields Too Large</h1>");
		return false;
	}

	// ---- locate end of headers ---------------------------------------------
	hdrEnd = m_clientBuffers[fd].find("\r\n\r\n");
	if (hdrEnd == std::string::npos)
		return false; // keep reading headers

	return true;
}

bool SocketManager::enforceHeaderLimits(int fd, size_t hdrEnd)
{
	// ---- post-parse header size caps (to catch huge single headers) ---------
	const size_t HEADER_CAP = 32 * 1024;
	const size_t MAX_HEADER_LINE = 8 * 1024;  // per-line limit

	// total header block size (up to but not including the blank line)
	if (hdrEnd > HEADER_CAP)
	{
		queueErrorAndClose(*this, fd, 431, "Request Header Fields Too Large",
						   "<h1>431 Request Header Fields Too Large</h1>");
		return false;
	}

	// per-line length cap
	size_t pos = 0;
	while (pos < hdrEnd)
	{
		size_t nl = m_clientBuffers[fd].find("\r\n", pos);
		if (nl == std::string::npos || nl > hdrEnd)
			break;
		if (nl - pos > MAX_HEADER_LINE)
		{
			queueErrorAndClose(*this, fd, 431, "Request Header Fields Too Large",
							   "<h1>431 Request Header Fields Too Large</h1>");
			return false;
		}
		pos = nl + 2;
	}

	// ---- header line-count cap (after headers complete) ---------------------
	const size_t MAX_HEADER_LINES = 100;
	size_t lines = 0;
	pos = 0;
	while (pos < hdrEnd)
	{
		size_t nl = m_clientBuffers[fd].find("\r\n", pos);
		if (nl == std::string::npos || nl > hdrEnd)
			break;
		++lines;
		if (lines > MAX_HEADER_LINES)
		{
			queueErrorAndClose(*this, fd, 431, "Request Header Fields Too Large",
							   "<h1>431 Request Header Fields Too Large</h1>");
			return false;
		}
		pos = nl + 2;
	}

	// ---- duplicate/conflict checks from RAW header block (pre-parse) --------
	const std::string headerBlock = m_clientBuffers[fd].substr(0, hdrEnd);
	std::map<std::string, size_t> nameCounts;
	countHeaderNames(headerBlock, nameCounts);
	if (nameCounts["content-length"] > 1)
	{
		queueErrorAndClose(*this, fd, 400, "Bad Request",
						   "<h1>400 Bad Request</h1><p>Multiple Content-Length headers</p>");
		return false;
	}
	if (nameCounts["content-length"] >= 1 && nameCounts["transfer-encoding"] >= 1)
	{
		queueErrorAndClose(*this, fd, 400, "Bad Request",
						   "<h1>400 Bad Request</h1><p>Both Content-Length and Transfer-Encoding present</p>");
		return false;
	}

	return true;
}

bool SocketManager::parseAndValidateRequest(int fd, size_t hdrEnd, Request &req,
											const ServerConfig* &server,
											std::string &methodUpper,
											size_t &contentLength,
											bool &hasTE)
{
	// ---- parse request line + headers once ----------------------------------
	req = parseRequest(m_clientBuffers[fd].substr(0, hdrEnd + 4));

	// normalize header keys BEFORE any framing/CL/TE validation
	normalizeHeaderKeys(req.headers);

	methodUpper = toUpperCopy(req.method);

	// Safe server mapping (non-throwing path shown; use your helper/try-version if you have it)
	const ServerConfig* serverPtr = 0;
	try
	{
		serverPtr = &findServerForClient(fd);
	}
	catch (...)
	{
		queueErrorAndClose(*this, fd, 400, "Bad Request",
						   "<h1>400 Bad Request</h1><p>No server mapping for this connection</p>");
		return false;
	}
	server = serverPtr;

	// ---- numeric CL/TE validation (overflow-safe) ---------------------------
	contentLength = 0;
	hasTE = false;
	int framingErr = validateBodyFraming(req.headers, contentLength, hasTE);
	if (framingErr != 0)
	{
		queueErrorAndClose(*this, fd, 400, "Bad Request",
						   "<h1>400 Bad Request</h1><p>Invalid Content-Length / Transfer-Encoding</p>");
		return false;
	}

	if (hasTE)
	{
		std::map<std::string, std::string>::const_iterator itTE = req.headers.find("transfer-encoding");
		if (itTE != req.headers.end())
		{
			std::string teVal = toLowerCopy(itTE->second);
			// trim spaces
			while (!teVal.empty() && (teVal[0] == ' ' || teVal[0] == '\t'))
				teVal.erase(0, 1);
			while (!teVal.empty() && (teVal[teVal.size() - 1] == ' ' || teVal[teVal.size() - 1] == '\t'))
				teVal.erase(teVal.size() - 1);

			// Only "chunked" is supported, no stacked encodings
			if (teVal != "chunked")
			{
				queueErrorAndClose(*this, fd, 400, "Bad Request",
								   "<h1>400 Bad Request</h1><p>Unsupported Transfer-Encoding</p>");
				return false;
			}
		}
	}

	// ---- policy: client_max_body_size (skip when chunked) -------------------
	if (!hasTE && server->client_max_body_size > 0 && contentLength > server->client_max_body_size)
	{
		queueErrorAndClose(*this, fd, 413, "Payload Too Large",
						   "<h1>413 Payload Too Large</h1>");
		return false;
	}

	return true;
}

bool SocketManager::processFirstTimeHeaders(int fd, const Request &req,
											const ServerConfig &server,
											const std::string &methodUpper,
											bool hasTE,
											size_t contentLength)
{
	// ---- first-time per-FD header processing (limits, 405/411, etc.) --------
	if (m_headersDone[fd])
		return true;

	m_headersDone[fd] = true;

	const RouteConfig* route = findMatchingLocation(server, req.path);

	// resolve per-FD allowed max body (route overrides server)
	size_t allowed = 0;
	if (route && route->max_body_size > 0)
		allowed = route->max_body_size;
	else if (server.client_max_body_size > 0)
		allowed = server.client_max_body_size;
	m_allowedMaxBody[fd] = allowed;

	// early 405 (pre-body)
	if (route && !route->allowed_methods.empty() &&
		!isMethodAllowedForRoute(methodUpper, route->allowed_methods))
	{
		std::set<std::string> allowSet = normalizeAllowedForAllowHeader(route->allowed_methods);
		std::string allowHeader = joinAllowedMethods(allowSet);

		Response res;
		res.status_code = 405;
		res.status_message = "Method Not Allowed";
		res.headers["Allow"] = allowHeader;
		res.headers["Content-Type"] = "text/html; charset=utf-8";
		res.body = "<h1>405 Method Not Allowed</h1>";
		res.headers["Content-Length"] = to_string(res.body.length());
		finalizeAndQueue(fd, res);
		return false;
	}

	// expectations from validated framing
	m_expectedContentLen[fd] = (!hasTE ? contentLength : 0);
	m_isChunked[fd] = hasTE;

	// 411: POST/PUT must provide length (CL or chunked)
	if ((methodUpper == "POST" || methodUpper == "PUT") &&
		!m_isChunked[fd] && m_expectedContentLen[fd] == 0)
	{
		Response res;
		res.status_code = 411;
		res.status_message = "Length Required";
		res.headers["Content-Type"] = "text/html; charset=utf-8";
		res.body = "<h1>411 Length Required</h1>";
		res.headers["Content-Length"] = to_string(res.body.length());
		finalizeAndQueue(fd, res);
		return false;
	}

	std::map<std::string, std::string>::const_iterator itExp = req.headers.find("expect");
	if (itExp != req.headers.end())
	{
		// Only HTTP/1.1 defines Expect: 100-continue semantics; ignore on HTTP/1.0.
		const bool isHTTP11 = (req.http_version.size() >= 8 &&
							   req.http_version.compare(0, 8, "HTTP/1.1") == 0);

		if (isHTTP11)
		{
			// Normalize the value; if there are multiple expectations, keep first token.
			std::string expectVal = toLowerCopy(itExp->second);
			size_t comma = expectVal.find(',');
			if (comma != std::string::npos)
				expectVal.erase(comma);
			// trim simple spaces/tabs
			while (!expectVal.empty() && (expectVal[0] == ' ' || expectVal[0] == '\t'))
				expectVal.erase(0, 1);
			while (!expectVal.empty() && (expectVal[expectVal.size() - 1] == ' ' || expectVal[expectVal.size() - 1] == '\t'))
				expectVal.erase(expectVal.size() - 1);

			// RFC: we only recognize token "100-continue". Anything else -> 417.
			if (expectVal == "100-continue" || !expectVal.empty())
			{
				Response res;
				res.status_code = 417;                      // Expectation Failed
				res.status_message = "Expectation Failed";
				res.headers["content-type"] = "text/plain; charset=utf-8";
				res.body = "417 Expectation Failed\n";
				res.headers["Content-Length"] = to_string(res.body.length());

				// No body is (or will be) consumed here.
				std::cerr << "[EXPECT] 417 on fd " << fd
						  << " http=" << req.http_version
						  << " expect='" << itExp->second << "'\n";

				finalizeAndQueue(fd, res);
				return false; // stop processing this request
			}
			// (If value empty, fall through and ignore — extremely unlikely in practice.)
		}
		// HTTP/1.0: ignore Expect entirely.
	}

	return true;
}

bool SocketManager::ensureBodyReady(int fd, size_t hdrEnd, size_t &requestEnd)
{
	// ---- body accounting / streaming limits ---------------------------------
	size_t bodyStart = hdrEnd + 4;
	size_t bodyBytes = (m_clientBuffers[fd].size() > bodyStart)
					   ? (m_clientBuffers[fd].size() - bodyStart) : 0;

	requestEnd = bodyStart; // default when no body is expected

	// 413 streaming cap
	if (m_allowedMaxBody[fd] > 0 && bodyBytes > m_allowedMaxBody[fd])
	{
		queueErrorAndClose(*this, fd, 413, "Payload Too Large",
						   "<h1>413 Payload Too Large</h1>");
		return false;
	}

	// 400 if body exceeds declared Content-Length
	if (m_expectedContentLen[fd] > 0 && bodyBytes > m_expectedContentLen[fd])
	{
		queueErrorAndClose(*this, fd, 400, "Bad Request",
						   "<h1>400 Bad Request</h1><p>Body exceeds Content-Length</p>");
		return false;
	}

	// wait for full CL body (if any)
	if (m_expectedContentLen[fd] > 0 && bodyBytes < m_expectedContentLen[fd])
		return false;
	if (m_expectedContentLen[fd] > 0)
		requestEnd = bodyStart + m_expectedContentLen[fd];

	// wait for chunked terminator (placeholder until real decoder)
	if (m_isChunked[fd])
	{
		size_t endMarkerPos = std::string::npos;
		if (m_clientBuffers[fd].size() >= bodyStart + 4)
			endMarkerPos = m_clientBuffers[fd].find("0\r\n\r\n", bodyStart);
		if (endMarkerPos == std::string::npos)
			return false;
		requestEnd = endMarkerPos + 5; // length of "0\r\n\r\n"
	}

	return true;
}

void SocketManager::dispatchRequest(int fd, const Request &req,
									const ServerConfig &server,
									const std::string &methodUpper)
{
	// ---- dispatch: static/autoindex/redirect --------------------------------
	const RouteConfig* route = findMatchingLocation(server, req.path);

	std::string effectiveRoot  = (route && !route->root.empty())  ? route->root  : server.root;
	std::string effectiveIndex = (route && !route->index.empty()) ? route->index : server.index;

	// strip route prefix
	std::string strippedPath = req.path;
	if (route && strippedPath.find(route->path) == 0)
		strippedPath = strippedPath.substr(route->path.length());
	if (!strippedPath.empty() && strippedPath[0] != '/')
		strippedPath = "/" + strippedPath;

	std::string fullPath = effectiveRoot + strippedPath;


	if (dirExists(fullPath))
	{
		// redirect missing trailing slash
		if (!req.path.empty() && req.path[req.path.length() - 1] != '/')
		{
			Response res;
			res.status_code = 301;
			res.status_message = "Moved Permanently";
			res.headers["Location"] = req.path + "/";
			res.headers["Content-Length"] = "0";
			const bool body_expected = false;
			const bool body_fully_consumed = true;
			finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
			return;
		}

		// try index
		std::string indexCandidate = fullPath;
		if (!indexCandidate.empty() && indexCandidate[indexCandidate.size() - 1] != '/')
			indexCandidate += '/';
		indexCandidate += effectiveIndex;

		if (fileExists(indexCandidate))
		{
			std::string body = readFile(indexCandidate);
			Response res;
			res.status_code = 200;
			res.status_message = "OK";
			res.body = body;
			res.headers["Content-Type"] = getMimeTypeFromPath(indexCandidate);
			res.headers["Content-Length"] = to_string(body.length());
			if (methodUpper == "HEAD")
				res.body.clear();
			const bool body_expected = false;
			const bool body_fully_consumed = true;
			finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
			return;
		}

		// autoindex
		if (route && route->autoindex)
		{
			std::string html = generateAutoIndexPage(fullPath, req.path);
			Response res;
			res.status_code = 200;
			res.status_message = "OK";
			res.body = html;
			res.headers["Content-Type"] = "text/html; charset=utf-8";
			res.headers["Content-Length"] = to_string(html.length());
			if (methodUpper == "HEAD")
				res.body.clear();
			const bool body_expected = false;
			const bool body_fully_consumed = true;
			finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
			return;
		}
	}

	// path safety / existence
	if (!isPathSafe(effectiveRoot, fullPath))
	{
		Response res = makeHtmlError(403, "Forbidden", "<h1>403 Forbidden</h1>");
		const bool body_expected = false;
		const bool body_fully_consumed = true;
		finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
		return;
	}
	if (!fileExists(fullPath))
	{
		Response res = makeHtmlError(404, "Not Found", "<h1>404 Not Found</h1>");
		const bool body_expected = false;
		const bool body_fully_consumed = true;
		finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
		return;
	}

	// static file 200
	std::string body = readFile(fullPath);
	Response res;
	res.status_code = 200;
	res.status_message = "OK";
	res.body = body;
	res.headers["Content-Type"] = getMimeTypeFromPath(fullPath);
	res.headers["Content-Length"] = to_string(body.length());
	if (methodUpper == "HEAD")
		res.body.clear();
	const bool body_expected = false;
	const bool body_fully_consumed = true;
	finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
}

void SocketManager::resetRequestState(int fd)
{
	m_headersDone[fd] = false;
	m_expectedContentLen[fd] = 0;
	m_allowedMaxBody[fd] = 0;
	m_isChunked[fd] = false;
}

/*----------------------------HERE IS THE NEW HANDLECLIENTTEAD v3 refactorised ------------------------------------------------
	Inside handleClientRead(fd) now:
	Step 0. If we already have something queued to write to that client (found in m_clientWriteBuffers[fd]), we bail early and DO NOT read more.
	→ So we are "1 request at a time per connection" and we don't pipeline.
	Step 1. readIntoClientBuffer(fd)
	nonblocking recv into m_clientBuffers[fd].
	handles disconnect (0 bytes or fatal error) by calling handleClientDisconnect(fd).

	Step 2. Header handling
	locateHeaders: find \r\n\r\n. Also enforces an in-progress header size cap (32KB) and if you exceed it before headers complete, you immediately queue 431 + close.
	Once headers are found:
	enforceHeaderLimits:
	total header block size limit (32KB)
	per-line max (8KB)
	total number of header lines (<=100)
	rejects multiple Content-Length headers
	rejects CL+TE together
	→ On violation: we build an error Response, mark close_connection, queue it via finalizeAndQueue(), and stop.

	Step 3. Parse the request
	parseAndValidateRequest:
	parses request line + headers into Request.
	normalizes header keys to lowercase.
	looks up which ServerConfig applies to this fd using findServerForClient(fd) (so virtualhost-ish mapping).
	checks Content-Length:
	only digits
	overflow safe
	1 CL -> 400
	CL+TE -> 400
	checks Transfer-Encoding:
	only allows exactly chunked. Anything else → 400.
	enforces server-wide client_max_body_size if it's NOT chunked and CL is too large → 413.
	If mapping server fails, we 400 and close.

	Step 4. First-time header-side policy for this FD
	processFirstTimeHeaders:
	Runs only once per new request (gated by m_headersDone[fd]).
	figures out which location/RouteConfig we're under (via findMatchingLocation), and records:
	per-route/per-server max_body_size into m_allowedMaxBody[fd]
	expected body length in m_expectedContentLen[fd]
	whether request is chunked in m_isChunked[fd]
	Enforces 405 Method Not Allowed EARLY if method not in route’s allowed list.
	-> Builds a 405 with an Allow: header, queues it, stops.
	Enforces 411 Length Required:
	if method is POST or PUT, and there's neither CL nor chunked, respond 411.
	Enforces Expect: header:
	If HTTP/1.1 and Expect: present and it's anything (including 100-continue), we currently send 417 Expectation Failed and stop.

	Step 5. Body readiness
	ensureBodyReady:
	figures out how many body bytes we already have after the header.
	live-stream check against m_allowedMaxBody[fd] → if exceeded, send 413 and close.
	if we advertised a Content-Length, makes sure we didn't already read more than that → 400 and close.
	waits until we actually received the full body:
	if CL given: require full CL bytes in buffer.
	if chunked: require to see the terminator "0\r\n\r\n" (temporary logic).
	If body not complete yet: just return and wait for more POLLIN.
	If body is complete, it returns requestEnd = end index of this full request in the buffer.

	Step 6. Dispatch
	dispatchRequest does:
	routing logic (root / index / autoindex / redirect)
	path traversal protection (isPathSafe)
	301 redirect if dir missing trailing slash
	200 static file or autoindex listing, etc.
	403, 404, 200, etc
	For HEAD, it clears the body before queueing.
	Calls finalizeAndQueue() with flags like body_expected/body_fully_consumed.

	Step 7. After dispatch
	We slice the processed request out of m_clientBuffers[fd] using erase(0, requestEnd) so leftover bytes remain in buffer (→ this is how we start to support multiple requests on the same TCP connection, sequentially).
	Then resetRequestState(fd) resets m_headersDone, body tracking, etc.
	So: this is now a full HTTP/1.1-ish state machine with:
	header caps + per-line caps
	CL overflow safety
	Content-Length vs. Transfer-Encoding correctness
	body size enforcement (server/global + route override)
	early 405/411/413/417 decisions
	support for POST and PUT with bodies
	preliminary chunked support (terminator check)
	keep-alive style pipelining of sequential requests on one connection
*/

/*
	New clienRead again, should be the last one:
	basically now we use ClientState as a connection phase !
	We organize based on ClientState with 3 helpers:
		clientHasPendingWrite
		readintoBuffer
	Basically before we assumed we had everything as soon as we saw Headers,
	it works for get put not for post/put etc... 

*/

bool SocketManager::clientHasPendingWrite(int fd) const
{
	std::map<int, std::string>::const_iterator it = m_clientWriteBuffers.find(fd);
		return (it != m_clientWriteBuffers.end() && !it->second.empty());
}

// Returns true only when the request body is fully read and st.phase is set to READY_TO_DISPATCH.
// Returns false when we need more data OR when an error response was queued (SENDING_RESPONSE).
bool SocketManager::tryReadBody(int fd, ClientState &st)
{
	// --- CHUNKED TRANSFER ---------------------------------------------------
	if (st.isChunked)
	{
		while (!st.recvBuffer.empty())
		{
			// Feed current buffer slice to the chunk decoder
			size_t consumed = 0;
			std::string produced;

			// TODO: adapt this call to your actual ChunkedDecoder API
			// Expected behavior:
			//  - append decoded bytes for completed chunks into `produced`
			//  - set `consumed` to how many bytes were read from input
			//  - return a status enum: OK (need more), DONE, or ERROR
			ChunkedDecoder::Status status =
				st.chunkDec.feed(st.recvBuffer.data(),
				                 st.recvBuffer.size(),
				                 produced,
				                 consumed);

			// Append newly decoded bytes to the body
			if (!produced.empty())
			{
				st.bodyBuffer.append(produced);
				// enforce streaming cap for chunked
				if (st.maxBodyAllowed > 0 && st.bodyBuffer.size() > st.maxBodyAllowed)
				{
					Response err = makeHtmlError(413, "Payload Too Large",
						"<h1>413 Payload Too Large</h1>");
					finalizeAndQueue(fd, err);
					st.phase = ClientState::SENDING_RESPONSE;
					return false;
				}
			}

			// Drop consumed bytes from recvBuffer
			if (consumed > 0)
				st.recvBuffer.erase(0, consumed);

			// State checks
			if (status == ChunkedDecoder::S_ERROR)
			{
				Response err = makeHtmlError(400, "Bad Request",
					"<h1>400 Bad Request</h1><p>Malformed chunked body.</p>");
				finalizeAndQueue(fd, err);
				st.phase = ClientState::SENDING_RESPONSE;
				return false;
			}
			if (status == ChunkedDecoder::S_DONE)
			{
				// Done decoding the body; any surplus in recvBuffer is the next request (pipelining)
				st.phase = ClientState::READY_TO_DISPATCH;
				return true;
			}

			// If decoder didn’t consume anything, we need more bytes from the socket
			if (consumed == 0 && produced.empty())
				return false;
		}

		// recvBuffer drained but decoder still needs more input
		return false;
	}

	// --- CONTENT-LENGTH -----------------------------------------------------
	// No chunked TE: read exactly st.contentLength bytes into bodyBuffer
	const size_t want = st.contentLength;
	const size_t haveNow = st.bodyBuffer.size();

	if (want <= haveNow)
	{
		// Already complete (shouldn't generally happen here, but be defensive)
		st.phase = ClientState::READY_TO_DISPATCH;
		return true;
	}

	if (!st.recvBuffer.empty())
	{
		size_t need = want - haveNow;
		size_t take = st.recvBuffer.size() < need ? st.recvBuffer.size() : need;

		if (take > 0)
		{
			st.bodyBuffer.append(st.recvBuffer.data(), take);
			st.recvBuffer.erase(0, take);

			// Enforce cap in CL mode (early 413 should have run already, this is a safety net)
			if (st.maxBodyAllowed > 0 && st.bodyBuffer.size() > st.maxBodyAllowed)
			{
				Response err = makeHtmlError(413, "Payload Too Large",
					"<h1>413 Payload Too Large</h1>");
				finalizeAndQueue(fd, err);
				st.phase = ClientState::SENDING_RESPONSE;
				return false;
			}
		}
	}

	// Check completion
	if (st.bodyBuffer.size() >= want)
	{
		// Body complete — any remaining st.recvBuffer is pipelined next request
		st.phase = ClientState::READY_TO_DISPATCH;
		return true;
	}

	// Need more bytes
	return false;
}

void SocketManager::handleClientRead(int fd)
{
	std::map<int, ClientState>::iterator it = m_clients.find(fd);
	if (it == m_clients.end())
		return ;

	ClientState &st = it->second;

	//dont read if we are still busy sending message
	if (clientHasPendingWrite(fd))
		return ;
	// 1. pull fresh bytes from revc() into st.recvBuffer
	if (!readIntoBuffer(fd, st))
	{
		handleClientDisconnect(fd);
		return;
	}

	// 2. advance state machine
	if (st.phase == ClientState::READING_HEADERS)
	{
		if (!tryParseHeaders(fd, st))
		 // tryParseHeaders:
        // - look for \r\n\r\n in st.recvBuffer
        // - if not complete yet: return true
        // - if complete: fill st.req, set st.isChunked/contentLength/maxBodyAllowed,
        //               strip header bytes from st.recvBuffer,
        //               set st.phase to READING_BODY or READY_TO_DISPATCH
        // - if bad request: queue error response + set st.phase = SENDING_RESPONSE, return false
			return; //need more data or we already have an error
	}

	if (st.phase == ClientState::READING_BODY)
	{
		// tryReadBody:
        // - if chunked: feed st.recvBuffer into st.chunkDec, append decoded to st.bodyBuffer
        // - if content-length: append recvBuffer to bodyBuffer
        // - enforce maxBodyAllowed
        // - when finished: set st.phase = READY_TO_DISPATCH
        // - on error (413, 400...): queue error response, st.phase=SENDING_RESPONSE, return false
		if (!tryReadBody(fd, st))
			return ; // need more data or we already have an error
	}
	if (st.phase == ClientState::READY_TO_DISPATCH)
	{
		finalizeRequestAndQueueResponse(fd, st);
		        // finalizeRequestAndQueueResponse() should:
        // - build & queue Response
        // - set st.phase = SENDING_RESPONSE
        // - maybe prep keep-alive state
	}

}

void SocketManager::handleClientRead(int fd)
{
	std::map<int, std::string>::const_iterator pending = m_clientWriteBuffers.find(fd);
	if (pending != m_clientWriteBuffers.end() && !pending->second.empty())
		return; // wait until current response is flushed

	if (!readIntoClientBuffer(fd))
		return;

	size_t hdrEnd = 0;
	if (!locateHeaders(fd, hdrEnd))
		return;

	if (!enforceHeaderLimits(fd, hdrEnd))
		return;

	Request req;
	const ServerConfig* server = 0;
	std::string methodUpper;
	size_t contentLength = 0;
	bool hasTE = false;
	if (!parseAndValidateRequest(fd, hdrEnd, req, server, methodUpper, contentLength, hasTE))
		return;

	if (!processFirstTimeHeaders(fd, req, *server, methodUpper, hasTE, contentLength))
		return;

	size_t requestEnd = 0;
	if (!ensureBodyReady(fd, hdrEnd, requestEnd))
		return;

	dispatchRequest(fd, req, *server, methodUpper);

	if (requestEnd > 0 && requestEnd <= m_clientBuffers[fd].size())
		m_clientBuffers[fd].erase(0, requestEnd);
	else
		m_clientBuffers[fd].clear();

	resetRequestState(fd);
}

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
	// Use MSG_NOSIGNAL to prevent SIGPIPE from killing the process if the client
	// disconnects while we're sending. On systems without this flag (e.g. macOS/BSD),
	// the call falls back to a normal send() and relies on EPIPE handling instead.V
	#ifdef MSG_NOSIGNAL
		ssize_t n = ::send(fd,buffer.data(), toSend, MSG_NOSIGNAL);
	#else
		ssize_t n = ::send(fd, buffer.data(), toSend, 0); // ::send <---- because we want to use send the system call and not some SocketManager::sent
	#endif


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
		if (buffer.empty())
		{
				// --- NEW LOGIC: check if we should keep-alive or close ---
				m_clientWriteBuffers.erase(it);
				std::map<int, bool>::iterator itFlag = m_closeAfterWrite.find(fd);
				const bool mustClose = (itFlag != m_closeAfterWrite.end() && itFlag->second);
				if (itFlag != m_closeAfterWrite.end())
						m_closeAfterWrite.erase(itFlag);

				if (mustClose)
				{
						handleClientDisconnect(fd);
				}
				else
				{
						// Keep the connection alive for the next request
						clearPollout(fd);            // stop monitoring for write events
						resetRequestState(fd);
						// If the client already pipelined another request, process it now.
						if (!m_clientBuffers[fd].empty())
								handleClientRead(fd);
						// otherwise wait for the next POLLIN event
				}
		}
}

void SocketManager::handleClientDisconnect(int fd)
{
	std::cout << "Disconnecting fd " << fd << std::endl;
	::close(fd);
	m_clientBuffers.erase(fd);
	m_clientWriteBuffers.erase(fd); //will go away
	m_clientToServerIndex.erase(fd);
	for (size_t i = 0; i < m_pollfds.size(); ++i)
	{
		if (m_pollfds[i].fd == fd)
		{
			m_pollfds.erase(m_pollfds.begin() + i);
			break ; 
		}
	}
	//old legacy code, will go away now that that we do per ClientState
	m_headersDone.erase(fd);
	m_expectedContentLen.erase(fd);
	m_allowedMaxBody.erase(fd);
	m_isChunked.erase(fd);

	m_clients.erase(fd);
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
/* 
const ServerConfig& SocketManager::findServerForClient(int fd) const
{
	std::map<int, size_t>::const_iterator it = m_clientToServerIndex.find(fd);
	if (it == m_clientToServerIndex.end())
		throw std::runtime_error("No matching server config for client FD");
	return m_config.servers[it->second];
}
 */

const ServerConfig& SocketManager::findServerForClient(int fd) const
{
	std::map<int, size_t>::const_iterator it = m_clientToServerIndex.find(fd);
	if (it == m_clientToServerIndex.end())
		throw std::runtime_error("No matching server config for client FD");

	const size_t idx = it->second;
	if (idx >= m_serversConfig.size())
		throw std::runtime_error("Server index out of range for client FD");

	return m_serversConfig[idx]; // ← use the same vector you map into
}

void SocketManager::finalizeAndQueue(int fd, const Request &req, Response &res, bool body_expected, bool body_fully_consumed)
{
	const bool headers_complete = true; 
	const bool client_close = clientRequestedClose(req);

	const bool close_it = shouldCloseAfterThisResponse(res.status_code, headers_complete, body_expected, body_fully_consumed, client_close);
	res.close_connection = close_it;

	m_clientWriteBuffers[fd] = build_http_response(res);
	setPollToWrite(fd);
	m_closeAfterWrite[fd] = close_it;
	/* std::cerr << "[KEEPALIVE] http=" << req.http_version
		  << " conn='" << (req.headers.count("connection") ? req.headers.find("connection")->second : std::string("<none>"))
		  << "' -> client_close=" << client_close
		  << " close_it=" << close_it << "\n"; */
}

//needed an overload of finalize
void SocketManager::finalizeAndQueue(int fd, Response &res)
{
	// Without a parsed Request, we can't honor per-request keep-alive safely.
	// Default to closing the connection after this response.
	const bool headers_complete       = true;
	const bool body_expected          = false;
	const bool body_fully_consumed    = true;
	const bool client_close           = true; // <- force close in no-context paths

	const bool close_it = shouldCloseAfterThisResponse(
		/*status_code*/ res.status_code,
		headers_complete,
		body_expected,
		body_fully_consumed,
		client_close
	);
	res.close_connection = close_it;

	m_clientWriteBuffers[fd] = build_http_response(res);
	setPollToWrite(fd);

	// remember for drain phase
	m_closeAfterWrite[fd] = close_it;
}


bool SocketManager::shouldCloseAfterThisResponse(int status_code, bool headers_complete, bool body_expected, bool body_fully_consumed, bool client_close) const
{
	(void)status_code;
	if (client_close)
		return true;
	if (!headers_complete)
		return true;
	if (body_expected && !body_fully_consumed)
		return true;
	return false;
}



bool SocketManager::clientRequestedClose(const Request &req) const
{
	// Parse HTTP version from the request line string like "HTTP/1.1"
	const std::string &hv = req.http_version;
	const bool is11 = (hv.size() >= 8 && hv.compare(0, 8, "HTTP/1.1") == 0);
	const bool is10 = (hv.size() >= 8 && hv.compare(0, 8, "HTTP/1.0") == 0);

	// Header keys are lowercased at parse time (Step 1); use "connection"
	std::string connVal;
	std::map<std::string, std::string>::const_iterator it = req.headers.find("connection");
	if (it != req.headers.end())
		connVal = toLowerCopy(trimCopy(it->second));  // normalize value

	// Split on comma; check first token only (common clients send "close" or "keep-alive")
	size_t comma = connVal.find(',');
	if (comma != std::string::npos)
		connVal.erase(comma); // keep first token
	connVal = trimCopy(connVal);

	if (is11)
		return (connVal == "close");               // HTTP/1.1: keep-alive by default
	if (is10)
		return (connVal != "keep-alive");          // HTTP/1.0: close unless explicitly keep-alive

	// Unknown/other versions: be safe and close
	return true;
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