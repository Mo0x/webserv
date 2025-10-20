/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:22 by mgovinda          #+#    #+#             */
/*   Updated: 2025/10/16 21:10:38 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SOCKETMANAGER_HPP
#define SOCKETMANAGER_HPP

#include "ServerSocket.hpp"
#include "utils.hpp"
#include "Config.hpp"
#include "request_parser.hpp"
#include <vector>
#include <poll.h>
#include <map>
#include <string>
#include <set> // NEW

class SocketManager
{
	private:
	std::vector<ServerSocket*> m_servers;
	std::vector<struct pollfd> m_pollfds;
	std::set<int>                  m_serverFds;     // to distinguish server vs client
    std::map<int, std::string>     m_clientBuffers; // client fd → partial data
	std::map<int, std::string>	m_clientWriteBuffers;
	std::vector<ServerConfig> m_serversConfig;
	Config	m_config;

	std::map<int, size_t> m_clientToServerIndex;

	//for max body size:
	std::map<int, bool>		m_headersDone;
	std::map<int, size_t>	m_expectedContentLen;
	std::map<int, size_t>	m_allowedMaxBody;

	SocketManager &operator=(const SocketManager &src);
	SocketManager(const SocketManager &src);
	std::map<int, bool> m_isChunked;

	//for keep-alive connection
	std::map<int, bool> m_closeAfterWrite;
	
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
};

#endif