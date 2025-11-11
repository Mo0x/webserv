/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:34 by mgovinda          #+#    #+#             */
/*   Updated: 2025/11/11 18:28:23 by mgovinda         ###   ########.fr       */
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
#include <cstring> //for std::strerror
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
//debug func
static const char* phaseToStr(ClientState::Phase p)
{
	switch (p)
	{
		case ClientState::READING_HEADERS:   return "READING_HEADERS";
		case ClientState::READING_BODY:      return "READING_BODY";
		case ClientState::READY_TO_DISPATCH: return "READY_TO_DISPATCH";
		case ClientState::SENDING_RESPONSE:  return "SENDING_RESPONSE";
		case ClientState::CLOSED:            return "CLOSED";
	}
	return "?";
}
void SocketManager::setPhase(int fd,
                            ClientState &st,
                            ClientState::Phase newp,
                            const char* where)
{
    if (st.phase != newp) {
        std::cerr << "[fd " << fd << "] phase " << phaseToStr(st.phase)
                  << " -> " << phaseToStr(newp)
                  << " at " << where << std::endl;
    }
    st.phase = newp;
}

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
	sockaddr_storage sa;
	socklen_t        slen = sizeof(sa);

	int client_fd = accept(listen_fd, reinterpret_cast<sockaddr*>(&sa), &slen);
	if (client_fd < 0)
	{
		// EAGAIN/EWOULDBLOCK is fine for nonblocking listen sockets
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			std::cerr << "accept() failed: " << std::strerror(errno) << std::endl;
		return;
	}

	// Make client socket nonblocking (avoid relying on external helpers)
	int flags = fcntl(client_fd, F_GETFL, 0);
	if (flags != -1)
		fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

	std::cout << "Accepted new client: fd " << client_fd << std::endl;

	// --- Register with poll() for reads (keep your existing poll vector logic)
	struct pollfd pdf;
	pdf.fd = client_fd;
	pdf.events = POLLIN;     // ready for reading
	pdf.revents = 0;
	m_pollfds.push_back(pdf);

	// --- Legacy per-fd maps (keep them until full migration)
	m_clientBuffers[client_fd]      = "";
	m_headersDone[client_fd]        = false;
	m_expectedContentLen[client_fd] = 0;
	m_allowedMaxBody[client_fd]     = 0;
	m_isChunked[client_fd]          = false;

	// --- Map this client to the correct server index (reuse your existing way)
	for (size_t i = 0; i < m_servers.size(); ++i)
	{
		if (m_servers[i]->getFd() == listen_fd)
		{
			m_clientToServerIndex[client_fd] = i;
			break;
		}
	}

	// --- NEW: insert refactored per-connection state so handleClientRead() works
        ClientState st = ClientState();
        setPhase(client_fd, st, ClientState::READING_HEADERS, "handleNewConnection");
        st.recvBuffer     = std::string();
	st.bodyBuffer     = std::string();
	st.isChunked      = false;
	st.contentLength  = 0;
	st.maxBodyAllowed = 0;
        st.writeBuffer.clear();
        st.forceCloseAfterWrite = false;
        st.closing = false;
        st.isMultipart = false;
        st.multipartInit = false;
        st.multipartBoundary.clear();
        st.mpState = ClientState::MP_START;
        st.mp = MultipartStreamParser();
        st.mpCtx = MultipartCtx();
        // If your ChunkedDecoder exposes a reset(), call it; otherwise this is a no-op line.

        m_clients[client_fd] = st;

	// If you track per-fd pending-write flags/queues, clear them here to avoid
	// the "skip read: pending write" early-return on fresh connections.
	// e.g. m_writeQueue.erase(client_fd); or clearPendingWriteFor(client_fd);
	// (leave as a comment if you don’t have such a structure)
	// clearPendingWriteFor(client_fd);

	// Optional: immediate trace so you’ll see the state machine start on next loop
	std::cerr << "[fd " << client_fd << "] inserted in m_clients, phase=READING_HEADERS" << std::endl;
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

void SocketManager::queueErrorAndClose(int fd, int status,
                                                           const std::string &title,
                                                           const std::string &html)
{
        std::map<int, ClientState>::iterator it = m_clients.find(fd);
        if (it == m_clients.end())
        {
                ClientState fresh = ClientState();
                setPhase(fd, fresh, ClientState::READING_HEADERS, "queueErrorAndClose");
                fresh.forceCloseAfterWrite = false;
                fresh.closing = false;
                m_clients[fd] = fresh;
                it = m_clients.find(fd);
        }
        ClientState &st = it->second;
        if (st.closing)
                return;
        st.closing = true;
        Response res = makeHtmlError(status, title, html);
        res.close_connection = true;     // invalid headers → must close
        finalizeAndQueue(fd, res);
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
	Step 0. If we already have something queued to write to that client (when st.writeBuffer is not empty), we bail early and DO NOT read more.
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

bool SocketManager::clientHasPendingWrite(const ClientState &st) const
{
	return !st.writeBuffer.empty();
}

// Returns true only when the request body is fully read and st.phase is set to READY_TO_DISPATCH.
// Returns false when we need more data OR when an error response was queued (SENDING_RESPONSE).


bool SocketManager::tryReadBody(int fd, ClientState &st)
{
        if (st.phase != ClientState::READING_BODY) {
                std::cerr << "[fd " << fd << "] BUG: tryReadBody called in phase=" << phaseToStr(st.phase)
                          << " — bug in call site\n";
        }

	// --- CHUNKED TRANSFER ---------------------------------------------------
	if (st.isChunked)
	{
		for (;;)
		{
			size_t consumed = 0;
			if (!st.recvBuffer.empty())
			{
				const size_t max_allowed = (st.maxBodyAllowed ? st.maxBodyAllowed : ~size_t(0));
				consumed = st.chunkDec.feed(st.recvBuffer.data(),
											st.recvBuffer.size(),
											max_allowed);
				if (consumed > 0)
					st.recvBuffer.erase(0, consumed);
			}

                        const size_t before = st.bodyBuffer.size();
                        st.chunkDec.drainTo(st.bodyBuffer);
                        const size_t drained = st.bodyBuffer.size() - before;

                        std::cerr << "[fd " << fd << "] chunked step: consumed=" << consumed
                                  << " drained=" << drained
                                  << " done=" << (st.chunkDec.done() ? 1 : 0)
                                  << " recv=" << st.recvBuffer.size()
                                  << " body=" << st.bodyBuffer.size() << std::endl;

			if (st.maxBodyAllowed > 0 && st.bodyBuffer.size() > st.maxBodyAllowed)
			{
				st.closing = true;
				st.recvBuffer.clear();
#ifdef SHUT_RD
				::shutdown(fd, SHUT_RD);
#endif
				Response err = makeHtmlError(413, "Payload Too Large",
											"<h1>413 Payload Too Large</h1>");
				finalizeAndQueue(fd, st.req, err, false, true);
				setPhase(fd, st, ClientState::SENDING_RESPONSE, "tryReadBody");
				return false;
			}

			if (st.chunkDec.hasError())
			{
				Response err = makeHtmlError(400, "Bad Request",
											"<h1>400 Bad Request</h1><p>Malformed chunked body.</p>");
				finalizeAndQueue(fd, st.req, err, false, true);
				setPhase(fd, st, ClientState::SENDING_RESPONSE, "tryReadBody");
				return false;
			}

                        if (st.chunkDec.done())
                        {
                                setPhase(fd, st, ClientState::READY_TO_DISPATCH, "tryReadBody");
                                return true;
                        }

			// No progress this tick → wait for more bytes.
                        if (consumed == 0 && drained == 0)
                        {
                                std::cerr << "[fd " << fd << "] chunked step: no progress, waiting for more" << std::endl;
                                return false;
                        }
			// else loop again (we made progress)
		}
	}

	// --- CONTENT-LENGTH -----------------------------------------------------
	// No chunked TE: read exactly st.contentLength bytes into bodyBuffer
	const size_t want = st.contentLength;
	const size_t haveNow = st.bodyBuffer.size();

        if (want <= haveNow)
        {
                // Already complete (shouldn't generally happen here, but be defensive)
                setPhase(fd, st, ClientState::READY_TO_DISPATCH, "tryReadBody");
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
				setPhase(fd, st, ClientState::SENDING_RESPONSE, "tryReadBody");
				return false;
			}
		}
	}

	// Check completion
        if (st.bodyBuffer.size() >= want)
        {
                // Body complete — any remaining st.recvBuffer is pipelined next request
                setPhase(fd, st, ClientState::READY_TO_DISPATCH, "tryReadBody");
                return true;
        }
	// Need more bytes
	return false;
}



// need to flesh it out once post/upload and cgi routig is ready
void SocketManager::finalizeRequestAndQueueResponse(int fd, ClientState &st)
{
	const ServerConfig &server = m_serversConfig[m_clientToServerIndex[fd]];
	// Get/Head using previous dispatcher for (static/autoindex/redirect)
	if (st.req.method == "GET" || st.req.method == "HEAD")
	{
		dispatchRequest(fd, st.req, server, st.req.method);
		return;
	}


	if (st.req.method == "POST")
	{
		const RouteConfig *route = findMatchingLocation(server, st.req.path);
		handlePostUploadOrCgi(fd, st.req, server, route, st.bodyBuffer);
		return;
	}
	// DELETE to WIRE HERE NEXT

    // if (st.req.method == "DELETE") {
    //     handleDeleteLegacy(fd, st.req, server);
    //     st.phase = ClientState::SENDING_RESPONSE;
    //     return;
    // }

	//Fallback
	std::cerr << "[fd " << fd << "] finalizeRequestAndQueueResponse enter" << std::endl;
	Response res;
	res.status_code = 501;
	res.status_message = "Not Implemented";
	res.headers["Content-Type"] = "text/html; charset=utf-8";
	res.body = "<h1>501 Not Implemented</h1>";
	res.headers["Content-Length"] = to_string(res.body.size());
	res.close_connection = false;

	finalizeAndQueue(fd, res);
	std::cerr << "[fd " << fd << "] queued fallback response" << std::endl;
}

void SocketManager::handleClientRead(int fd)
{
	std::map<int, ClientState>::iterator it = m_clients.find(fd);
	if (it == m_clients.end())
		return ;

	ClientState &st = it->second;

	if (st.closing)
	{
	        return;
	}

	std::cerr << "[fd " << fd << "] enter handleClientRead phase=" << phaseToStr(st.phase)
	          << " recv=" << st.recvBuffer.size() << " body=" << st.bodyBuffer.size() << std::endl;

	// Flush any pending response before reading more request data.
	if (st.phase == ClientState::SENDING_RESPONSE || clientHasPendingWrite(st))
	{
		if (!tryFlushWrite(fd, st))
			return;
	}

	// -------- BODY-FIRST fast path ------------------------------------------
	// If we're already in READING_BODY, first try to progress with what we have
	// before attempting another recv(). This prevents stalls when the whole body
	// (e.g., a complete chunked message) arrived in the previous read.
	if (st.phase == ClientState::READING_BODY)
	{
		// tryReadBody:
		// - if chunked: feed st.recvBuffer into st.chunkDec, append decoded to st.bodyBuffer
		// - if content-length: append recvBuffer to bodyBuffer
		// - enforce maxBodyAllowed
		// - when finished: set st.phase = READY_TO_DISPATCH
		// - on error (413, 400...): queue error response, st.phase=SENDING_RESPONSE, return false
		if (tryReadBody(fd, st))
		{
			// Body may now be complete → fall through to READY_TO_DISPATCH check below
		}
		else
		{
			// Need more bytes → now attempt a read
			if (!readIntoBuffer(fd, st))
			{
				// Peer closed or fatal read. If CL case is exactly satisfied, allow dispatch;
				// otherwise treat as incomplete body.
				if (!st.isChunked && st.contentLength == st.bodyBuffer.size())
				{
						setPhase(fd, st, ClientState::READY_TO_DISPATCH, "handleClientRead");
				}
				else
				{
					Response err = makeHtmlError(400, "Bad Request",
						"<h1>400 Bad Request</h1><p>Unexpected close during request body.</p>");
					finalizeAndQueue(fd, st.req, err, /*body_expected=*/false, /*body_fully_consumed=*/true);
					setPhase(fd, st, ClientState::SENDING_RESPONSE, "handleClientRead");
					return;
				}
			}
			else
			{
				// Got more bytes; try to progress again
				if (!tryReadBody(fd, st))
					return; // need more data; wait for next read event
			}
		}
	}
	else
	{
		// -------- HEADER / NORMAL path --------------------------------------
		// 1. pull fresh bytes from recv() into st.recvBuffer
		if (!readIntoBuffer(fd, st))
		{
			handleClientDisconnect(fd);
			return;
		}
		std::cerr << "[fd " << fd << "] after readIntoBuffer recv=" << st.recvBuffer.size() << std::endl;

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
				return; // need more data or we already have an error
		}
		// If we just transitioned to READING_BODY, try to consume immediately
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
	}

	// 3) Dispatch if ready
	if (st.phase == ClientState::READY_TO_DISPATCH)
	{
		std::cerr << "[fd " << fd << "] READY_TO_DISPATCH -> finalizeRequestAndQueueResponse" << std::endl;
		finalizeRequestAndQueueResponse(fd, st);
		// finalizeRequestAndQueueResponse() should:
		// - build & queue Response
		// - set st.phase = SENDING_RESPONSE
		// - maybe prep keep-alive state
		return;
	}
}

void SocketManager::handleClientWrite(int fd)
{
	std::map<int, ClientState>::iterator it = m_clients.find(fd);
	if (it == m_clients.end())
		return;

	tryFlushWrite(fd, it->second);
}


void SocketManager::handleClientDisconnect(int fd)
{
	std::cout << "Disconnecting fd " << fd << std::endl;
	::close(fd);
	m_clientBuffers.erase(fd);
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

	std::map<int, ClientState>::iterator it = m_clients.find(fd);
	if (it != m_clients.end())
		setPhase(fd, it->second, ClientState::CLOSED, "handleClientDisconnect");
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
	ClientState &st = m_clients[fd];
	const bool headers_complete = true;
	const bool client_close = clientRequestedClose(req);

        const bool close_it = shouldCloseAfterThisResponse(res.status_code, headers_complete, body_expected, body_fully_consumed, client_close);
        const bool force_close = close_it || st.closing;
        res.close_connection = force_close;

        st.writeBuffer = build_http_response(res);
        st.forceCloseAfterWrite = force_close;
	setPhase(fd, st, ClientState::SENDING_RESPONSE, "finalizeAndQueue");
	tryFlushWrite(fd, st);
}

//needed an overload of finalize
void SocketManager::finalizeAndQueue(int fd, Response &res)
{
	ClientState &st = m_clients[fd];
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
        const bool force_close = close_it || st.closing;
        res.close_connection = force_close;

        st.writeBuffer = build_http_response(res);
        st.forceCloseAfterWrite = force_close;
	setPhase(fd, st, ClientState::SENDING_RESPONSE, "finalizeAndQueue");
	tryFlushWrite(fd, st);
}


bool SocketManager::tryFlushWrite(int fd, ClientState &st)
{
	while (!st.writeBuffer.empty())
	{
		size_t toSend = st.writeBuffer.size();
	#ifdef MSG_NOSIGNAL
		ssize_t n = ::send(fd, st.writeBuffer.data(), toSend, MSG_NOSIGNAL);
	#else
		ssize_t n = ::send(fd, st.writeBuffer.data(), toSend, 0);
	#endif
		if (n < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			{
				setPollToWrite(fd);
				return false;
			}
			handleClientDisconnect(fd);
			return false;
		}
		if (n == 0)
		{
			setPollToWrite(fd);
			return false;
		}
		st.writeBuffer.erase(0, static_cast<size_t>(n));
	}

	clearPollout(fd);
	st.writeBuffer.clear();

	if (st.forceCloseAfterWrite || st.closing)
	{
			handleClientDisconnect(fd);
			return false;
	}

	st.forceCloseAfterWrite = false;
	st.closing = false;
	resetRequestState(fd);
	st.req = Request();
        st.bodyBuffer.clear();
        st.isChunked = false;
        st.contentLength = 0;
        st.maxBodyAllowed = 0;
        st.chunkDec = ChunkedDecoder();
        st.isMultipart = false;
        st.multipartInit = false;
        st.multipartBoundary.clear();
        st.mpState = ClientState::MP_START;
        st.mp = MultipartStreamParser();
        st.mpCtx = MultipartCtx();
        setPhase(fd, st, ClientState::READING_HEADERS, "tryFlushWrite");
        return true;
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

