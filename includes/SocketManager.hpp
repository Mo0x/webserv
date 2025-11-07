/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:22 by mgovinda          #+#    #+#             */
/*   Updated: 2025/11/07 15:54:49 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SOCKETMANAGER_HPP
#define SOCKETMANAGER_HPP

#include "ServerSocket.hpp"
#include "utils.hpp"
#include "Config.hpp"
#include "request_parser.hpp"
#include "Chunked.hpp"
#include <vector>
#include <poll.h>
#include <map>
#include <string>
#include <set>

struct ClientState
{
	enum Phase
	{
		READING_HEADERS,
		READING_BODY,
		READY_TO_DISPATCH,
		SENDING_RESPONSE,
		CLOSED
	};

	Phase			phase;

	// raw bytes undread from socket
	std::string		recvBuffer;

	// parsed request line + header
	Request			req;
	
	// Body framing
	bool			isChunked;
	size_t			contentLength;
	size_t			maxBodyAllowed;

	//decoded body (for post, cgi stdin, uploads)
	std::string		bodyBuffer;

	//our decoder stated when TE: chunked
	ChunkedDecoder	chunkDec;

	// pending response payload
	std::string		writeBuffer;
	bool			forceCloseAfterWrite;
};

class SocketManager
{
	private:
	std::vector<ServerSocket*>	m_servers;
	std::vector<struct pollfd>	m_pollfds;
	std::set<int>				m_serverFds;     // to distinguish server vs client
    std::map<int, std::string> 	m_clientBuffers; // client fd → partial data
	std::map<int, ClientState>	m_clients;
	std::vector<ServerConfig>	m_serversConfig;
	Config						m_config;

	std::map<int, size_t>		m_clientToServerIndex;

	//for max body size:
	std::map<int, bool>			m_headersDone;
	std::map<int, size_t>		m_expectedContentLen;
	std::map<int, size_t>		m_allowedMaxBody;

	SocketManager &operator=(const SocketManager &src);
	SocketManager(const SocketManager &src);
	std::map<int, bool> m_isChunked;

	public:
	SocketManager(const Config &config);
	SocketManager();
	~SocketManager();

	void addServer(const std::string& host, unsigned short port);
	void initPoll();
	void run(); // infinite poll loop

	// member func inside run()
	bool isListeningSocket(int fd) const;
	void handleNewConnection(int listen_fd);
	void handleClientRead(int fd);
	void handleClientDisconnect(int fd);
	void handleClientWrite(int fd);
	std::string buildErrorResponse(int code, const ServerConfig &server);
	const ServerConfig& findServerForClient(int fd) const;
	void setPollToWrite(int fd);
	void clearPollout(int fd);
	bool clientRequestedClose(const Request &req) const;

	void setServers(const std::vector<ServerConfig> & servers);
	
	void finalizeAndQueue(int fd, const Request &req, Response &res, bool body_expected, bool body_fully_consumed);
	void finalizeAndQueue(int fd, Response &res);
	//for keep-alive
	bool shouldCloseAfterThisResponse(int status_code, bool headers_complete, bool body_expected, bool body_fully_consumed, bool client_close) const;

	private:

	bool readIntoBuffer(int fd, ClientState &st);
	bool readIntoClientBuffer(int fd); // probably to delete once the new handleClientRead works

	bool locateHeaders(int fd, size_t &hdrEnd);
	bool enforceHeaderLimits(int fd, size_t hdrEnd);
	bool parseAndValidateRequest(int fd, size_t hdrEnd, Request &req,
									const ServerConfig* &server,
									std::string &methodUpper,
									size_t &contentLength,
									bool &hasTE);
	bool processFirstTimeHeaders(int fd, const Request &req,
									const ServerConfig &server,
									const std::string &methodUpper,
									bool hasTE,
									size_t contentLength);
	bool ensureBodyReady(int fd, size_t hdrEnd, size_t &requestEnd);
	void dispatchRequest(int fd, const Request &req,
							const ServerConfig &server,
							const std::string &methodUpper);
	void resetRequestState(int fd);
	bool clientHasPendingWrite(const ClientState &st) const;

	bool tryParseHeaders(int fd, ClientState &st);
	bool checkHeaderLimits(int fd, ClientState &st, size_t &hdrEndPos);
	Response makeHtmlError(int code, const std::string& reason, const std::string& html);
	bool parseRawHeadersIntoRequest(int fd, ClientState &st, size_t hdrEndPos);
	bool applyRoutePolicyAfterHeaders(int fd, ClientState &st);
	bool badRequestAndQueue(int fd, ClientState &st);
	bool setupBodyFramingAndLimits(int fd, ClientState &st);
	void finalizeHeaderPhaseTransition (int fd, ClientState &st, size_t hdrEndPos);
	bool tryReadBody(int fd, ClientState &st);
	void queueErrorAndClose(SocketManager &sm, int fd, int status, const std::string &title, const std::string &html);
	void finalizeRequestAndQueueResponse(int fd, ClientState &st);
	bool tryFlushWrite(int fd, ClientState &st);

	void	handlePostUploadOrCgi(int fd, 
									const Request &req,
									const ServerConfig &server,
									const RouteConfig *route,
									const std::string &body);
	void setPhase(int fd,
                            ClientState &st,
                            ClientState::Phase newp,
                            const char* where);
	

};

#endif
