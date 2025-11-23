/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:22 by mgovinda          #+#    #+#             */
/*   Updated: 2025/11/23 19:01:22 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SOCKETMANAGER_HPP
#define SOCKETMANAGER_HPP

#include <map>
#include <poll.h>
#include <set>
#include <string>
#include <vector>

#include "Chunked.hpp"
#include "Config.hpp"
#include "MultipartStreamParser.hpp"
#include "ServerSocket.hpp"
#include "utils.hpp"

// -------------------------- Multipart context -------------------------------
struct MultipartCtx
{
	std::vector<std::string>   savedNames;
	std::map<std::string,std::string> fields;
	std::string                fieldName, safeFilename, safeFilenameRaw, fieldBuffer, pendingWrite, currentFilePath;
	size_t                     partBytes, partCount, totalDecoded;
	int                        fileFd;
	bool                       writingFile;

	MultipartCtx() : partBytes(0), partCount(0), totalDecoded(0), fileFd(-1), writingFile(false) {}
};

// ------------------------------- CGI state ----------------------------------
struct Cgi
{
	pid_t pid;
	int stdin_w;
	int stdout_r;
	bool stdin_closed;
	bool stdoutPaused;
	std::string inBuf;  // decoded request body to feed child
	std::string outBuf; // bytes read but not yet parsed
	bool headersParsed;
	int cgiStatus;
	std::map<std::string, std::string> cgiHeaders;
	size_t bytesInTotal, bytesOutTotal;
	unsigned long long tStartMs;
	std::string scriptFsPath;
	std::string workingDir;

	Cgi();
	void reset();
};

// ----------------------------- Client state ---------------------------------
struct ClientState
{
	enum Phase
	{
		READING_HEADERS,
		READING_BODY,
		READY_TO_DISPATCH,
		CGI_RUNNING,
		SENDING_RESPONSE,
		CLOSED
	};

	enum MpState
	{
		MP_START,
		MP_PARSING,
		MP_DONE,
		MP_ERROR
	};

	Phase                 phase;
	std::string           recvBuffer;     // raw bytes unread from socket
	Request               req;            // parsed request line + headers
	bool                  isChunked;
	size_t                contentLength;
	size_t                maxBodyAllowed;
	std::string           bodyBuffer;     // decoded body (uploads/CGI stdin)
	ChunkedDecoder        chunkDec;
	std::string           writeBuffer;    // pending response payload
	bool                  forceCloseAfterWrite;
	bool                  closing;

	// Multipart
	bool                  isMultipart;
	bool                  multipartInit;
	std::string           multipartBoundary;
	MpState               mpState;
	MultipartStreamParser mp;
	MultipartCtx          mpCtx;
	size_t                debugMultipartBytes;
	std::string           uploadDir;
	size_t                maxFilePerPart;
	bool                  multipartError;
	int                   multipartStatusCode;
	std::string           multipartStatusTitle;
	std::string           multipartStatusBody;

	// CGI
	struct Cgi            cgi;

	bool mpDone() const;

	ClientState();
};

// ================================ Manager ===================================
class SocketManager
{
	public:
	SocketManager(const Config &config);
	SocketManager();
	~SocketManager();

	// Lifecycle
	void addServer(const std::string& host, unsigned short port);
	void setServers(const std::vector<ServerConfig> & servers);
	void initPoll();
	void run();

	
	private:
	// Event loop handlers
	bool isListeningSocket(int fd) const;
	void handleNewConnection(int listen_fd);
	void handleClientRead(int fd);
	void handleClientDisconnect(int fd);
	void handleClientWrite(int fd);

	// Connection / server context
	//std::string buildErrorResponse(int code, const ServerConfig &server); REPLACING BY A NEW ONE THAT RETURN A RESPONSE
	const ServerConfig& findServerForClient(int fd) const;
	bool clientRequestedClose(const Request &req) const;

	// Poll bookkeeping
	void setPollToWrite(int fd);
	void clearPollout(int fd);

	// Response queueing / keep-alive
	void finalizeAndQueue(int fd, const Request &req, Response &res, bool body_expected, bool body_fully_consumed);
	void finalizeAndQueue(int fd, Response &res);
	bool shouldCloseAfterThisResponse(int status_code, bool headers_complete, bool body_expected, bool body_fully_consumed, bool client_close) const;
	// Core sockets and poll bookkeeping
	std::vector<ServerSocket*>	m_servers;
	std::vector<struct pollfd>	m_pollfds;
	std::set<int>				m_serverFds;
	std::vector<ServerConfig>	m_serversConfig;
	Config						m_config;

	// Per-client state
	std::map<int, ClientState>	m_clients;
	std::map<int, size_t>		m_clientToServerIndex;

	// CGI pipe ↔ client associations
	std::map<int, int> m_cgiStdoutToClient;
	std::map<int, int> m_cgiStdinToClient;

	SocketManager &operator=(const SocketManager &src);
	SocketManager(const SocketManager &src);

	// IO helpers
	bool readIntoBuffer(int fd, ClientState &st);
	bool tryFlushWrite(int fd, ClientState &st);
	bool clientHasPendingWrite(const ClientState &st) const;

	// Legacy HTTP parsing helpers (still used in HTTP pipeline)
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

	// HTTP pipeline
	bool tryParseHeaders(int fd, ClientState &st);
	bool checkHeaderLimits(int fd, ClientState &st, size_t &hdrEndPos);
	Response makeHtmlError(int code, const std::string& reason, const std::string& html);
	Response makeConfigErrorResponse(const ServerConfig &server,
													const RouteConfig  *route,
													int                 code,
													const std::string  &reason,
													const std::string  &fallbackHtml);
	bool parseRawHeadersIntoRequest(int fd, ClientState &st, size_t hdrEndPos);
	bool detectMultipartBoundary(int fd, ClientState &st);
	bool applyRoutePolicyAfterHeaders(int fd, ClientState &st);
	bool badRequestAndQueue(int fd, ClientState &st);
	bool setupBodyFramingAndLimits(int fd, ClientState &st);
	void finalizeHeaderPhaseTransition (int fd, ClientState &st, size_t hdrEndPos);
	bool tryReadBody(int fd, ClientState &st);
	void queueErrorAndClose(int fd, int status, const std::string &title, const std::string &html);

	// Dispatch to handlers
	void splitPathAndQuery(const std::string &raw, std::string &urlPath, std::string &query);
	void dispatchRequest(int fd, const Request &req,
							const ServerConfig &server,
							const std::string &methodUpper);
	void finalizeRequestAndQueueResponse(int fd, ClientState &st);

	void	handlePostUploadOrCgi(int fd, 
									const Request &req,
									const ServerConfig &server,
									const RouteConfig *route,
									const std::string &body);

	// DELETE handler
	void	handleDelete(int fd,
					const Request &req,
					const ServerConfig &server,
					const RouteConfig *route);

	// Multipart orchestration
	void setPhase(int fd,
	              ClientState &st,
	              ClientState::Phase newp,
	              const char* where);
	void resetMultipartState(ClientState &st);
	bool handleMultipartFailure(int fd, ClientState &st);
	void cleanupMultipartFiles(ClientState &st, bool unlinkSaved);
	static void setMultipartError(ClientState &st, int status, const std::string &title, const std::string &html);
	void teardownMultipart(ClientState &st, bool unlinkSaved);
	void queueMultipartSummary(int fd, ClientState &st);
	static void onPartBeginThunk(void* user, const std::map<std::string,std::string>& headers);
	static void onPartDataThunk(void* user, const char* buf, size_t n);
	static void onPartEndThunk(void* user);

	bool  doTheMultiPartThing(int fd, ClientState &st);
	bool  feedToMultipart(int fd, ClientState &st, const char* p, size_t n);

	std::string extractBoundary(const std::string& ct) const;
	bool  routeAllowsUpload(const ClientState& st) const;
	std::string generateUploadName(size_t index) const;
	void  queue201UploadList(int fd, const std::vector<std::string>& names);

	// CGI orchestration
	void startCgiDispatch(int fd,
						ClientState &st,
						const ServerConfig &server,
						const RouteConfig &route);
	bool tryCgiDispatchNow(int fd,
						ClientState &st,
						const ServerConfig &srv,
						const RouteConfig &route);
	void addPollFd(int fd, short events);
	void modPollEvents(int fd, short setMask, short clearMask);
	void delPollFd(int fd);
	bool isCgiStdout(int fd) const;
	bool isCgiStdin (int fd) const;
	void drainCgiOutput(int clienFd);
	void pauseCgiStdoutIfNeeded(int clientFd, ClientState &st);
	void maybeResumeCgiStdout(int clientFd, ClientState &st);
	bool parseCgiHeaders(ClientState &st, int clientFd, const RouteConfig &route);
	void killCgiProcess(ClientState &st, int sig);
	void reapCgiIfDone(ClientState &st);
	void handleCgiWritable(int pipefd);
	void handleCgiReadable(int pipefd);
	void handleCgiPipeError(int pipefd);
	void checkCgiTimeouts(); // new for cgi time out
};

#endif
