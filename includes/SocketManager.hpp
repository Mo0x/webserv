/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:22 by mgovinda          #+#    #+#             */
/*   Updated: 2025/06/09 18:19:40 by mgovinda         ###   ########.fr       */
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
	Config	m_config;
	SocketManager &operator=(const SocketManager &src);
	SocketManager(const SocketManager &src);

	public:
	SocketManager(const Config &config);
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
	std::string buildErrorResponse(int code);
};

#endif