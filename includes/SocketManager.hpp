/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:22 by mgovinda          #+#    #+#             */
/*   Updated: 2025/11/10 11:31:26 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SOCKETMANAGER_HPP
#define SOCKETMANAGER_HPP

#include "ServerSocket.hpp"
#include "utils.hpp"
#include "Config.hpp"
#include "request_parser.hpp"
#include "Chunked.hpp"
#include "FileUploadHandler.hpp"
#include "MultipartStreamParser.hpp"
#include <vector>
#include <poll.h>
#include <map>
#include <string>
#include <set>

struct MultipartCtx
{
	FileUploadHandler          file;
	std::vector<std::string>   savedNames;
	std::map<std::string,std::string> fields;
	std::string                fieldName, safeFilename, safeFilenameRaw, fieldBuffer, pendingWrite, currentFilePath;
	size_t                     partBytes, partCount, totalDecoded;
	int                        fileFd;
	bool                       writingFile;
	MultipartCtx() : partBytes(0), partCount(0), totalDecoded(0), fileFd(-1), writingFile(false) {}
};
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
	bool			closing;
	//Multi-Part
	enum MpState
	{
		MP_START,
		MP_PARSING,
		MP_DONE,
		MP_ERROR
	};

	bool isMultipart;
	bool multipartInit;
	std::string multipartBoundary;
	MpState	mpState;
	MultipartStreamParser mp;
	MultipartCtx mpCtx;
	size_t debugMultipartBytes;
	std::string uploadDir;
	size_t maxFilePerPart;
	bool multipartError;

	bool mpDone() const { return mp.isDone(); }

	ClientState()
		: phase(READING_HEADERS),
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
		  multipartError(false)
	{
	}
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

        // Legacy header/body pipeline helpers (preserved under srcs/legacy/).
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
        bool detectMultipartBoundary(int fd, ClientState &st);
        bool applyRoutePolicyAfterHeaders(int fd, ClientState &st);
	bool badRequestAndQueue(int fd, ClientState &st);
	bool setupBodyFramingAndLimits(int fd, ClientState &st);
	void finalizeHeaderPhaseTransition (int fd, ClientState &st, size_t hdrEndPos);
	bool tryReadBody(int fd, ClientState &st);
	void queueErrorAndClose(int fd, int status, const std::string &title, const std::string &html);
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
	void resetMultipartState(ClientState &st);
	bool handleMultipartFailure(int fd, ClientState &st);
	void cleanupMultipartFiles(ClientState &st, bool unlinkSaved);
	void queueMultipartSummary(int fd, ClientState &st);
	//wiring multipart
	static void onPartBeginThunk(void* user, const std::map<std::string,std::string>& headers);
	static void onPartDataThunk(void* user, const char* buf, size_t n);
	static void onPartEndThunk(void* user);

	//Multipart helpers
	bool  doTheMultiPartThing(int fd, ClientState &st);   // you already added
	bool  feedToMultipart(int fd, ClientState &st, const char* p, size_t n);

	std::string extractBoundary(const std::string& ct) const;
	bool  routeAllowsUpload(const ClientState& st) const;
	std::string generateUploadName(size_t index) const;
	void  queue201UploadList(int fd, const std::vector<std::string>& names);	

};

#endif
