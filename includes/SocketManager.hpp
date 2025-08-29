/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:22 by mgovinda          #+#    #+#             */
/*   Updated: 2025/08/29 20:17:27 by mgovinda         ###   ########.fr       */
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

	void setServers(const std::vector<ServerConfig> & servers);
};

#endif