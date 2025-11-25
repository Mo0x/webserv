/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:34 by mgovinda          #+#    #+#             */
/*   Updated: 2025/11/25 20:38:01 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "SocketManager.hpp"
#include "file_utils.hpp"
#include "request_response_struct.hpp"
#include "utils.hpp"
#include <csignal> // for clean shutdown when ctrl+c


extern volatile sig_atomic_t g_stop;
bool ClientState::mpDone() const
{
	return mp.isDone();
}

ClientState::ClientState() :
	phase(READING_HEADERS),
	recvBuffer(),
	req(),
	isChunked(false),
	contentLength(0),
	maxBodyAllowed(0),
	bodyBuffer(),
	chunkDec(),
	writeBuffer(),
	forceCloseAfterWrite(false),
	closing(false),
	isMultipart(false),
	multipartInit(false),
	multipartBoundary(),
	mpState(MP_START),
	mp(),
	mpCtx(),
	debugMultipartBytes(0),
	uploadDir(),
	maxFilePerPart(0),
	multipartError(false),
	multipartStatusCode(0),
	multipartStatusTitle(),
	multipartStatusBody()
{
	return ;
}

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
		case ClientState::CGI_RUNNING:       return "CGI_RUNNING";
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


static bool isCgiEndpoint(const RouteConfig &route, const std::string &urlPath)
{
	const std::string ext = getFileExtension(urlPath);
	return !ext.empty() && route.cgi_extension.find(ext) != route.cgi_extension.end();
}

bool SocketManager::tryCgiDispatchNow(int fd,
									  ClientState &st,
									  const ServerConfig &srv,
									  const RouteConfig &route)
{
	std::string urlPath;
	std::string query;
	splitPathAndQuery(st.req.path, urlPath, query);
	if (!isCgiEndpoint(route, urlPath))
		return false;

	startCgiDispatch(fd, st, srv, route); 
	return true;
}

void SocketManager::resetMultipartState(ClientState &st)
{
	if (st.mpCtx.fileFd >= 0)
	{
		::close(st.mpCtx.fileFd);
		st.mpCtx.fileFd = -1;
	}
	if (!st.mpCtx.currentFilePath.empty())
	{
		::unlink(st.mpCtx.currentFilePath.c_str());
		st.mpCtx.currentFilePath.clear();
	}
	st.mpCtx.currentFilePath.clear();
	st.mpCtx.savedNames.clear();
	st.mpCtx.fields.clear();
	st.mpCtx.fieldName.clear();
	st.mpCtx.safeFilename.clear();
	st.mpCtx.safeFilenameRaw.clear();
	st.mpCtx.fieldBuffer.clear();
	st.mpCtx.pendingWrite.clear();
	st.mpCtx.currentFilePath.clear();
	st.mpCtx.partBytes = 0;
	st.mpCtx.partCount = 0;
	st.mpCtx.totalDecoded = 0;
	st.mpCtx.writingFile = false;
	st.debugMultipartBytes = 0;
	st.multipartError = false;
	st.multipartStatusCode = 0;
	st.multipartStatusTitle.clear();
	st.multipartStatusBody.clear();
}

bool SocketManager::handleMultipartFailure(int fd, ClientState &st)
{
	if (!st.multipartError)
		return false;
	cleanupMultipartFiles(st, true);
	const int status = (st.multipartStatusCode != 0) ? st.multipartStatusCode : 413;
	std::string title;
	std::string body;
	if (!st.multipartStatusTitle.empty())
		title = st.multipartStatusTitle;
	else if (status == 500)
		title = "Internal Server Error";
	else if (status == 400)
		title = "Bad Request";
	else
		title = "Payload Too Large";

	if (!st.multipartStatusBody.empty())
		body = st.multipartStatusBody;
	else if (status == 500)
		body = "<h1>500 Internal Server Error</h1><p>Upload failed.</p>";
	else if (status == 400)
		body = "<h1>400 Bad Request</h1><p>Malformed multipart upload.</p>";
	else
		body = "<h1>413 Payload Too Large</h1><p>Multipart upload aborted.</p>";

	const ServerConfig &srv = findServerForClient(fd);
	const RouteConfig *rt = st.req.path.empty() ? NULL : findMatchingLocation(srv, st.req.path);
	Response err = makeConfigErrorResponse(
		srv,
		rt,
		status,
		title,
		body
	);
	finalizeAndQueue(fd, st.req, err, false, true);
	setPhase(fd, st, ClientState::SENDING_RESPONSE, "tryReadBody");
	return true;
}

void SocketManager::setMultipartError(ClientState &st, int status, const std::string &title, const std::string &html)
{
	if (st.multipartError)
		return;
	st.multipartError = true;
	st.multipartStatusCode = status;
	st.multipartStatusTitle = title;
	st.multipartStatusBody = html;
}

// p == data and n == size
bool SocketManager::feedToMultipart(int fd, ClientState &st, const char* p, size_t n)
{
	if (!st.isMultipart || n == 0)
		return true;

	const size_t nextTotal = st.mpCtx.totalDecoded + n;
	if (st.maxBodyAllowed > 0 && nextTotal > st.maxBodyAllowed)
	{
			const ServerConfig &srv = findServerForClient(fd);
			const RouteConfig *rt = st.req.path.empty() ? NULL : findMatchingLocation(srv, st.req.path);
			Response err = makeConfigErrorResponse(
					srv,
					rt,
					413,
					"Payload Too Large",
					"<h1>413 Payload Too Large</h1>"
			);
			finalizeAndQueue(fd, st.req, err, false, true);
			setPhase(fd, st, ClientState::SENDING_RESPONSE, "feedToMultipart");
			return false;
	}

	MultipartStreamParser::Result mpRes = st.mp.feed(p, n);
	st.mpCtx.totalDecoded = nextTotal;
	if (mpRes == MultipartStreamParser::ERR)
	{
			const ServerConfig &srv = findServerForClient(fd);
			const RouteConfig *rt = st.req.path.empty() ? NULL : findMatchingLocation(srv, st.req.path);
			Response err = makeConfigErrorResponse(
					srv,
					rt,
					400,
					"Malformed multipart body",
					"<h1>400 Malformed multipart body</h1>"
			);
			finalizeAndQueue(fd, st.req, err, false, true);
			setPhase(fd, st, ClientState::SENDING_RESPONSE, "feedToMultipart");
			return false;
	}

	if (handleMultipartFailure(fd, st))
		return false;

	return true;
}

void SocketManager::cleanupMultipartFiles(ClientState &st, bool unlinkSaved)
{
	if (st.mpCtx.fileFd >= 0)
	{
		::close(st.mpCtx.fileFd);
		st.mpCtx.fileFd = -1;
	}
	if (!st.mpCtx.currentFilePath.empty())
	{
		if (unlinkSaved)
			::unlink(st.mpCtx.currentFilePath.c_str());
		st.mpCtx.currentFilePath.clear();
	}
	if (unlinkSaved)
	{
		for (std::vector<std::string>::iterator it = st.mpCtx.savedNames.begin();
			 it != st.mpCtx.savedNames.end(); ++it)
			::unlink(it->c_str());
		st.mpCtx.savedNames.clear();
	}
}

void SocketManager::teardownMultipart(ClientState &st, bool unlinkSaved)
{
	cleanupMultipartFiles(st, unlinkSaved);
	resetMultipartState(st);
	st.isMultipart = false;
	st.multipartInit = false;
	st.multipartBoundary.clear();
	st.mpState = ClientState::MP_START;
}

void SocketManager::queueMultipartSummary(int fd, ClientState &st)
{
	std::ostringstream body;
	body << "Uploaded files:\n";
	if (st.mpCtx.savedNames.empty())
		body << "  (none)\n";
	else
	{
		for (size_t i = 0; i < st.mpCtx.savedNames.size(); ++i)
		{
			const std::string &p = st.mpCtx.savedNames[i];
			size_t slash = p.find_last_of('/');
			const std::string display = (slash == std::string::npos) ? p : p.substr(slash + 1);
			body << "  - " << display << "\n";
		}
	}
	body << "\nFields:\n";
	if (st.mpCtx.fields.empty())
		body << "  (none)\n";
	else
	{
		for (std::map<std::string,std::string>::const_iterator it = st.mpCtx.fields.begin();
			 it != st.mpCtx.fields.end(); ++it)
		{
			body << "  - " << it->first << ": " << it->second << "\n";
		}
	}

	Response res;
	res.status_code = 201;
	res.status_message = "Created";
	res.headers["Content-Type"] = "text/plain; charset=utf-8";
	res.body = body.str();
	res.headers["Content-Length"] = to_string(res.body.size());
	finalizeAndQueue(fd, st.req, res, false, true);
}

SocketManager::SocketManager()
{
	return ;
}

SocketManager::SocketManager(const SocketManager &src)
	: m_pollfds(src.m_pollfds),
	  m_serverFds(src.m_serverFds)
{
	// Do NOT copy m_servers — ServerSocket is non-copyable
}

SocketManager &SocketManager::operator=(const SocketManager &src)
{
	if (this != &src)
	{
		m_pollfds = src.m_pollfds;
		m_serverFds = src.m_serverFds;
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

	while (!g_stop)
	{
		struct pollfd* pbase = m_pollfds.empty() ? NULL : &m_pollfds[0];
		int rc = ::poll(pbase, static_cast<nfds_t>(m_pollfds.size()), 100); // changed from -1 to 100 for the time out
		if (rc < 0) 
		{
			if (errno == EINTR) 
			{
				if (g_stop)
					break;
				continue;
			}
			std::cerr << "poll() error: " << std::strerror(errno) << std::endl;
			continue;
		}
		if (rc == 0)
		{
			checkCgiTimeouts();
			continue;
		}

		std::vector< std::pair<int, short> > events;
		events.reserve(m_pollfds.size());
		for (size_t i = 0; i < m_pollfds.size(); ++i) 
		{
			if (m_pollfds[i].revents != 0)
				events.push_back(std::make_pair(m_pollfds[i].fd, m_pollfds[i].revents));
		}
		// event driver
		for (size_t i = 0; i < events.size(); ++i)
		{
			int   fd      = events[i].first;
			short revents = events[i].second;

			if (revents & POLLIN) 
			{
				if (isListeningSocket(fd)) 
				{
					handleNewConnection(fd);
				} 
				else if (isCgiStdout(fd)) 
				{
					handleCgiReadable(fd);
				} 
				else 
				{
					handleClientRead(fd);
				}
			}
			if (revents & POLLOUT) 
			{
				if (isCgiStdin(fd)) 
				{
					handleCgiWritable(fd);
				} else 
				{
					handleClientWrite(fd);
				}
			}
			if (revents & (POLLERR | POLLHUP | POLLNVAL)) 
			{
				if (isCgiStdout(fd) || isCgiStdin(fd)) 
				{
					handleCgiPipeError(fd);
				}
				else
				{
					handleClientDisconnect(fd);
				}
			}
		}
		checkCgiTimeouts();
	}
}

bool SocketManager::isListeningSocket(int fd) const
{
	return m_serverFds.count(fd) > 0;
}

void SocketManager::handleNewConnection(int listen_fd)
{
	sockaddr_storage	sa;
	socklen_t			slen = sizeof(sa);

	int client_fd = accept(listen_fd, reinterpret_cast<sockaddr*>(&sa), &slen);
	if (client_fd < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			std::cerr << "accept() failed: " << std::strerror(errno) << std::endl;
		return;
	}

	int flags = fcntl(client_fd, F_GETFL, 0);
	if (flags != -1)
		fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

	std::cout << "Accepted new client: fd " << client_fd << std::endl;

	struct pollfd pdf;
	pdf.fd = client_fd;
	pdf.events = POLLIN;     // ready for reading
	pdf.revents = 0;
	m_pollfds.push_back(pdf);

	for (size_t i = 0; i < m_servers.size(); ++i)
	{
		if (m_servers[i]->getFd() == listen_fd)
		{
			m_clientToServerIndex[client_fd] = i;
			break;
		}
	}

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
	resetMultipartState(st);

	m_clients[client_fd] = st;
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

		const ServerConfig &srv = findServerForClient(fd);
		const RouteConfig  *route = NULL;
		if (!st.req.path.empty())
			route = findMatchingLocation(srv, st.req.path);

		Response res = makeConfigErrorResponse(srv, route, status, title, html);
		res.close_connection = true;
		
		finalizeAndQueue(fd, res);
}

// Nonblocking read into the ClientState recvBuffer
bool SocketManager::readIntoBuffer(int fd, ClientState &st)
{
	char buffer[4096];
	ssize_t bytes = ::recv(fd, buffer, sizeof(buffer), 0);
	
	if (bytes == 0)
		return false;
	
	if (bytes <= 0) //we are dumb here, before we used erno but its after recv so this is clean from subject but dumb.
	{
		return false;
	}

	st.recvBuffer.append(buffer, static_cast<size_t>(bytes));
	return true;
}


void SocketManager::dispatchRequest(int fd, const Request &req,
									const ServerConfig &server,
									const std::string &methodUpper)
{
	// dispatch: static/autoindex/redirect 
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
		Response res = makeConfigErrorResponse(server, route, 403, "Forbidden", "<h1> 403 Forbidden</h1>");
			const bool body_expected = false;
		const bool body_fully_consumed = true;
		finalizeAndQueue(fd, req, res, body_expected, body_fully_consumed);
		return;
	}
	if (!fileExists(fullPath))
	{
			Response res = makeConfigErrorResponse(server, route, 404, "Not Found", "<h1>404 Not Found</h1>");
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

	if (handleMultipartFailure(fd, st))
		return false;

	// CHUNKED TRANSFER
	if (st.isChunked)
	{
		for (;;)
		{
			size_t consumed = 0;
			if (!st.recvBuffer.empty())
			{
				//~size_t(0) == clever trick to populate a size_t full of ones
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

			if (st.isMultipart && drained > 0)
			{
				const char* chunkPtr = st.bodyBuffer.data() + before;
				if (!feedToMultipart(fd, st, chunkPtr, drained))
					return false;
				st.bodyBuffer.resize(before);
			}
			else if (!st.isMultipart && st.maxBodyAllowed > 0 && st.bodyBuffer.size() > st.maxBodyAllowed)
			{
				st.closing = true;
				st.recvBuffer.clear();
#ifdef SHUT_RD
				::shutdown(fd, SHUT_RD);
#endif
				const ServerConfig &srv = findServerForClient(fd);
				const RouteConfig *rt = st.req.path.empty() ? NULL : findMatchingLocation(srv, st.req.path);
				Response err = makeConfigErrorResponse(srv,
														rt,
														413,
														"Payload Too Large",
														"<h1>413 Payload Too Large</h1>");
				finalizeAndQueue(fd, st.req, err, false, true);
				setPhase(fd, st, ClientState::SENDING_RESPONSE, "tryReadBody");
				return false;
			}

			if (st.chunkDec.hasError())
			{
				// makeHTML ? perhaps makeConfigErrorReponse need to check
				Response err = makeHtmlError(400, "Bad Request",
											"<h1>400 Bad Request</h1><p>Malformed chunked body.</p>");
				finalizeAndQueue(fd, st.req, err, false, true);
				setPhase(fd, st, ClientState::SENDING_RESPONSE, "tryReadBody");
				return false;
			}

			if (st.chunkDec.done())
			{
				if (st.isMultipart && !st.mpDone())
				{
					const ServerConfig &srv = findServerForClient(fd);
					const RouteConfig *rt = st.req.path.empty() ? NULL : findMatchingLocation(srv, st.req.path);
					Response err = makeConfigErrorResponse(
									srv,
									rt,
									400,
									"Bad Request",
									"<h1>400 Bad Request</h1><p>Multipart ended before closing boundary.</p>"
					);
					finalizeAndQueue(fd, st.req, err, false, true);
					setPhase(fd, st, ClientState::SENDING_RESPONSE, "tryReadBody");
					return false;
				}
				setPhase(fd, st, ClientState::READY_TO_DISPATCH, "tryReadBody");
				return true;
			}

			// No progress this tick → wait for more bytes.
			if (consumed == 0 && drained == 0)
			{
					std::cerr << "[fd " << fd << "] chunked step: no progress, waiting for more" << std::endl;
					return false;
			}
			// else loop again (we made progress) no need to code it
		}
	}
	// No chunked TE: read exactly st.contentLength bytes into bodyBuffer
	const size_t want = st.contentLength;
	const size_t haveNow = st.isMultipart ? st.mpCtx.totalDecoded : st.bodyBuffer.size();
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
			if (st.isMultipart)
			{
				const char* chunkPtr = st.recvBuffer.data();
				if (!feedToMultipart(fd, st, chunkPtr, take))
					return false;
				st.recvBuffer.erase(0, take);
			}
			else
			{
				st.bodyBuffer.append(st.recvBuffer.data(), take);
				st.recvBuffer.erase(0, take);

				if (st.maxBodyAllowed > 0 && st.bodyBuffer.size() > st.maxBodyAllowed)
				{
					const ServerConfig &srv = findServerForClient(fd);
					const RouteConfig *rt = st.req.path.empty() ? NULL : findMatchingLocation(srv, st.req.path);
					Response err = makeConfigErrorResponse(
								srv,
								rt,
								413,
								"Payload Too Large",
								"<h1>413 Payload Too Large</h1>");
					finalizeAndQueue(fd, st.req, err, false, true);
					setPhase(fd, st, ClientState::SENDING_RESPONSE, "tryReadBody");
					return false;
				}
			}
		}
	}

	if (handleMultipartFailure(fd, st))
		return false;

		// Check completion
	const size_t haveTotal = st.isMultipart ? st.mpCtx.totalDecoded : st.bodyBuffer.size();
	if (haveTotal >= want)
	{
		if (st.isMultipart && !st.mpDone())
		{
			const ServerConfig &srv = findServerForClient(fd);
			const RouteConfig *rt = st.req.path.empty() ? NULL : findMatchingLocation(srv, st.req.path);
			Response err = makeConfigErrorResponse(
										srv,
										rt,
										400,
										"Bad Request",
										"<h1>400 Bad Request</h1><p>Multipart ended before closing boundary.</p>");
			finalizeAndQueue(fd, st.req, err, false, true);
			setPhase(fd, st, ClientState::SENDING_RESPONSE, "tryReadBody");
			return false;
		}
		// Body complete — any remaining st.recvBuffer is pipelined next request
		setPhase(fd, st, ClientState::READY_TO_DISPATCH, "tryReadBody");
		return true;
	}
	// Need more bytes
	return false;
}



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
		if (st.isMultipart)
		{
			if (st.multipartError)
			{
				handleMultipartFailure(fd, st);
				return;
			}
			st.req.form_fields = st.mpCtx.fields;
			queueMultipartSummary(fd, st);
			return;
		}
		handlePostUpload(fd, st.req, server, route, st.bodyBuffer);
		return;
	}
	if (st.req.method == "DELETE")
	{
		const RouteConfig *route = findMatchingLocation(server, st.req.path);
		handleDelete(fd, st.req, server, route);
		return;
	}

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

	 if (st.phase == ClientState::CGI_RUNNING)
	{
		// While CGI is running, the client might:
		//  - close the connection (we must detect EOF)
		//  - pipeline the next request (we should buffer it)
		//
		// So treat POLLIN normally: read from the socket.
		if (!readIntoBuffer(fd, st))
			handleClientDisconnect(fd);
		//drainCgiOutput(fd);
		return;
	}

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
				bool completed = false;
				if (!st.isChunked)
				{
					const size_t have = st.isMultipart ? st.mpCtx.totalDecoded : st.bodyBuffer.size();
					if (st.contentLength == have && (!st.isMultipart || st.mpDone()))
					{
						setPhase(fd, st, ClientState::READY_TO_DISPATCH, "handleClientRead");
						completed = true;
					}
				}
				if (!completed)
				{
					const ServerConfig &srv = findServerForClient(fd);
					const RouteConfig *rt = st.req.path.empty() ? NULL : findMatchingLocation(srv, st.req.path);
					Response err = makeConfigErrorResponse(
							srv,
							rt,
							400,
							"Bad Request",
							"<h1>400 Bad Request</h1><p>Unexpected close during request body.</p>"
					);
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
		//Normal Path here
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

			if (st.phase == ClientState::READING_BODY && st.isMultipart && !st.multipartInit)
			{
				st.mp.reset(st.multipartBoundary,
							&SocketManager::onPartBeginThunk,
							&SocketManager::onPartDataThunk,
							&SocketManager::onPartEndThunk,
							&st);
				st.multipartInit = true;
				std::cout << "[fd" << fd << "] multipart parser reset";
			}
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
		const ServerConfig &srv = m_serversConfig[m_clientToServerIndex[fd]];

		std::string urlPath;
		std::string query;
		splitPathAndQuery(st.req.path, urlPath, query);

		const RouteConfig *rt = findMatchingLocation(srv, urlPath);

		if (rt && tryCgiDispatchNow(fd, st, srv, *rt))
			return;

		std::cerr << "[fd " << fd << "] READY_TO_DISPATCH -> finalizeRequestAndQueueResponse" << std::endl;
		finalizeRequestAndQueueResponse(fd, st);
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

	std::map<int, ClientState>::iterator it = m_clients.find(fd);
	if (it != m_clients.end())
		setPhase(fd, it->second, ClientState::CLOSED, "handleClientDisconnect");
	m_clients.erase(fd);
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
	if (st.isMultipart)
	{
		const bool unlinkSaved = (res.status_code >= 400);
		teardownMultipart(st, unlinkSaved);
	}

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
	if (st.isMultipart)
	{
		const bool unlinkSaved = (res.status_code >= 400);
		teardownMultipart(st, unlinkSaved);
	}
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

// redone because we were using send in a loop so it could go against the one sent per client/fd
// Note : On linux if you call send() on a TCP socket t whose peer 
// has already closed the connection, the kernel may raise SIGPIPE, 
// which by default kills the process. -> dont want this on a server ! 
//MSG_NOSIGNAL = this would normally raise SIGPIPE, don’t send the signal; just return -1 with errno = EPIPE
// now that we dont use errno idk if its useful, keeping it for the pre-proc style
bool SocketManager::tryFlushWrite(int fd, ClientState &st)
{

	if (st.writeBuffer.empty())
	{
		clearPollout(fd);
		if (st.forceCloseAfterWrite || st.closing)
		{
			handleClientDisconnect(fd);
			return false;
		}

		st.forceCloseAfterWrite = false;
		st.closing = false;
		setPhase(fd, st, ClientState::READING_HEADERS, "tryFlushWrite");
		return true;
	}

	// here we have just one send() from the subject and we dont check errno after it
#ifdef MSG_NOSIGNAL
	ssize_t n = ::send(fd, st.writeBuffer.data(), st.writeBuffer.size(), MSG_NOSIGNAL);
#else
	ssize_t n = ::send(fd, st.writeBuffer.data(), st.writeBuffer.size(), 0);
#endif
	if (n <= 0)
	{
		handleClientDisconnect(fd);
		return false;
	}

	// here we know we sent something so we delete that from writeBuffer
	st.writeBuffer.erase(0, static_cast<size_t>(n));

	if (st.phase == ClientState::CGI_RUNNING)
		maybeResumeCgiStdout(fd, st);

	// keep POLLOUT till we send everything
	if (!st.writeBuffer.empty())
	{
		setPollToWrite(fd);   // ensure POLLOUT is set
		return false;         // still flushing this response
	}
	clearPollout(fd);
	if (st.forceCloseAfterWrite || st.closing)
	{
		handleClientDisconnect(fd);
		return false;
	}

	st.forceCloseAfterWrite = false;
	st.closing = false;
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
	resetMultipartState(st);
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

	std::string connVal;
	std::map<std::string, std::string>::const_iterator it = req.headers.find("connection");
	if (it != req.headers.end())
		connVal = toLowerCopy(trimCopy(it->second));  // normalize value

	size_t comma = connVal.find(',');
	if (comma != std::string::npos)
		connVal.erase(comma); // keep first token
	connVal = trimCopy(connVal);

	if (is11)
		return (connVal == "close");// HTTP/1.1: keep-alive by default
	if (is10)
		return (connVal != "keep-alive");// HTTP/1.0: close unless explicitly keep-alive

	return true;
}

static std::string trimQuotes(const std::string &value)
{
	if (value.size() >= 2 && value[0] == '"' && value[value.size() - 1] == '"')
		return value.substr(1, value.size() - 2);
	return value;
}

static std::string sanitizeMultipartFilename(const std::string &raw)
{
	std::string base = raw;
	size_t slash = base.find_last_of("/\\");
	if (slash != std::string::npos)
		base = base.substr(slash + 1);
	std::string cleaned;
	for (size_t i = 0; i < base.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(base[i]);
		if (c < 32 || c == '/' || c == '\\')
			continue;
		cleaned.push_back(static_cast<char>(c));
		if (cleaned.size() >= 100)
			break;
	}
	if (cleaned.empty())
		cleaned = "upload.bin";
	return cleaned;
}

static std::string joinUploadPath(const std::string &dir, const std::string &name)
{
	if (dir.empty())
		return name;
	if (dir[dir.size() - 1] == '/')
		return dir + name;
	return dir + "/" + name;
}

static std::string makeUniqueUploadPath(const std::string &dir, const std::string &base)
{
	if (!fileExists(joinUploadPath(dir, base)))
		return joinUploadPath(dir, base);
	std::string stem = base;
	std::string ext;
	size_t dot = base.find_last_of('.');
	if (dot != std::string::npos)
	{
		stem = base.substr(0, dot);
		ext = base.substr(dot);
	}
	for (size_t i = 1; i < 1000; ++i)
	{
		std::ostringstream oss;
		oss << stem << "_" << i << ext;
		std::string candidate = joinUploadPath(dir, oss.str());
		if (!fileExists(candidate))
			return candidate;
	}
	return joinUploadPath(dir, stem + "_upload" + ext);
}

void SocketManager::onPartBeginThunk(void* user, const std::map<std::string,std::string>& headers)
{
	ClientState *cs = static_cast<ClientState*>(user);
	const size_t headerCount = headers.size();
	if (cs)
	{
		cs->mpCtx.partCount++;
		cs->mpCtx.fieldName.clear();
		cs->mpCtx.safeFilename.clear();
		cs->mpCtx.safeFilenameRaw.clear();
		cs->mpCtx.fieldBuffer.clear();
		cs->mpCtx.pendingWrite.clear();
		cs->mpCtx.currentFilePath.clear();
		cs->mpCtx.partBytes = 0;
		cs->mpCtx.writingFile = false;
		if (cs->mpCtx.fileFd >= 0)
		{
			::close(cs->mpCtx.fileFd);
			cs->mpCtx.fileFd = -1;
		}
	}

	std::string formName;
	std::string fileName;
	std::map<std::string,std::string>::const_iterator it = headers.find("content-disposition");
	if (it != headers.end())
	{
		std::string raw = trimCopy(it->second);
		bool firstToken = true;
		size_t pos = 0;
		while (pos < raw.size())
		{
			size_t semi = raw.find(';', pos);
			std::string token = raw.substr(pos, (semi == std::string::npos) ? std::string::npos : semi - pos);
			token = trimCopy(token);
			if (!token.empty())
			{
				if (firstToken)
				{
					firstToken = false;
				}
				else
				{
					size_t eq = token.find('=');
					std::string key = (eq == std::string::npos) ? token : token.substr(0, eq);
					std::string value = (eq == std::string::npos) ? std::string() : token.substr(eq + 1);
					key = toLowerCopy(trimCopy(key));
					value = trimCopy(value);
					if (!value.empty())
						value = trimQuotes(value);
					if (key == "name")
						formName = value;
					else if (key == "filename")
						fileName = value;
				}
			}
			if (semi == std::string::npos)
				break;
			pos = semi + 1;
		}
	}

	if (cs)
	{
		cs->mpCtx.fieldName = formName;
		cs->mpCtx.safeFilenameRaw = fileName;
		cs->mpCtx.writingFile = !fileName.empty();
	}

	if (cs && cs->mpCtx.writingFile && !cs->multipartError)
	{
		if (cs->uploadDir.empty() || !dirExists(cs->uploadDir))
		{
			std::cerr << "[multipart] upload dir unavailable: " << cs->uploadDir << std::endl;
			SocketManager::setMultipartError(*cs, 500, "Internal Server Error",
				"<h1>500 Internal Server Error</h1><p>Upload directory unavailable.</p>");
			cs->mpCtx.writingFile = false;
		}
		else
		{
			const std::string safe = sanitizeMultipartFilename(fileName);
			const std::string fullPath = makeUniqueUploadPath(cs->uploadDir, safe);
			int fd = ::open(fullPath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
			if (fd < 0)
			{
				std::cerr << "[multipart] failed to open " << fullPath << ": " << std::strerror(errno) << std::endl;
				SocketManager::setMultipartError(*cs, 500, "Internal Server Error",
					"<h1>500 Internal Server Error</h1><p>Unable to create upload file.</p>");
				cs->mpCtx.writingFile = false;
			}
			else
			{
				cs->mpCtx.fileFd = fd;
				cs->mpCtx.currentFilePath = fullPath;
				size_t slash = fullPath.find_last_of('/');
				cs->mpCtx.safeFilename = (slash == std::string::npos) ? fullPath : fullPath.substr(slash + 1);
				cs->mpCtx.partBytes = 0;
				std::cerr << "[multipart] writing file " << cs->mpCtx.currentFilePath << std::endl;
			}
		}
	}

	std::cerr << "[multipart] par begin headers=" << static_cast<unsigned long>(headerCount)
		<< "name=" << formName << "filename=" << fileName << std::endl;
}

void SocketManager::onPartDataThunk(void* user, const char* buf, size_t n)
{
	ClientState *cs = static_cast<ClientState*>(user);
	if (!cs)
		return;
	cs->debugMultipartBytes += n;

	//file case
	if (cs->mpCtx.writingFile && cs->mpCtx.fileFd >= 0 && !cs->multipartError)
	{
		size_t written = 0;
		while (written < n)
		{
			ssize_t w = ::write(cs->mpCtx.fileFd, buf + written, n - written);
			if (w <= 0)
			{
				std::cerr << "[multipart] write error while saving upload\n";
				SocketManager::setMultipartError(*cs, 500, "Internal Server Error",
					"<h1>500 Internal Server Error</h1><p>Failed while writing upload.</p>");
				break;
			}

			written += static_cast<size_t>(w);
			cs->mpCtx.partBytes += static_cast<size_t>(w);

			if (cs->maxFilePerPart > 0 && cs->mpCtx.partBytes > cs->maxFilePerPart)
			{
				std::cerr << "[multipart] part exceeded limit (" 
						  << static_cast<unsigned long>(cs->mpCtx.partBytes)
						  << " > " << static_cast<unsigned long>(cs->maxFilePerPart) << ")\n";
				SocketManager::setMultipartError(*cs, 413, "Payload Too Large",
					"<h1>413 Payload Too Large</h1><p>Upload exceeded allowed size.</p>");
				break;
			}
		}
	}
	//field case
	else if (!cs->multipartError)
	{
		const size_t CAP = 64 * 1024;
		size_t cur = cs->mpCtx.fieldBuffer.size();
		size_t room = (cur < CAP) ? (CAP - cur) : 0;
		size_t toCopy = room < n ? room : n;
		if (toCopy > 0)
			cs->mpCtx.fieldBuffer.append(buf, toCopy);
		if (toCopy < n)
		{
			std::cerr << "[multipart] field too large (>" 
					  << static_cast<unsigned long>(CAP) << " bytes)\n";
			SocketManager::setMultipartError(*cs, 413, "Payload Too Large",
				"<h1>413 Payload Too Large</h1><p>Form field exceeded allowed size.</p>");
		}
	}

	std::cerr << "[multipart] part data chunk =" << static_cast<unsigned long>(n)
			  << " total =" << static_cast<unsigned long>(cs->debugMultipartBytes)
			  << std::endl;
}

void SocketManager::onPartEndThunk(void* user)
{
	ClientState *cs = static_cast<ClientState*>(user);
	if (cs && cs->mpCtx.writingFile)
	{
		if (cs->mpCtx.fileFd >= 0)
		{
			::close(cs->mpCtx.fileFd);
			cs->mpCtx.fileFd = -1;
		}
		if (cs->multipartError)
		{
			if (!cs->mpCtx.currentFilePath.empty())
				::unlink(cs->mpCtx.currentFilePath.c_str());
		}
		else if (!cs->mpCtx.currentFilePath.empty())
		{
			cs->mpCtx.savedNames.push_back(cs->mpCtx.currentFilePath);
			std::cerr << "[multipart] saved " << cs->mpCtx.currentFilePath.c_str() << std::endl;
		}
		cs->mpCtx.currentFilePath.clear();
		cs->mpCtx.writingFile = false;
	}
	else if (cs)
	{
		if (!cs->multipartError && !cs->mpCtx.fieldName.empty())
			cs->mpCtx.fields[cs->mpCtx.fieldName] = cs->mpCtx.fieldBuffer;
		cs->mpCtx.fieldBuffer.clear();
		cs->mpCtx.fieldName.clear();
	}

	std::cerr << "[multipart] part end (count=" << static_cast<unsigned long>(cs ? cs->mpCtx.partCount : 0u)
	<< "totalBytes=" << static_cast<unsigned long>(cs ? cs->debugMultipartBytes : 0u) << ")" << std::endl;
}
