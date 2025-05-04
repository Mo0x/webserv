/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:22 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/16 19:25:12 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SOCKETMANAGER_HPP
#define SOCKETMANAGER_HPP

#include "ServerSocket.hpp"
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
	SocketManager &operator=(const SocketManager &src);
	SocketManager(const SocketManager &src);

	public:
	SocketManager();
	~SocketManager();

	void addServer(const std::string& host, unsigned short port);
	void initPoll();
	void run(); // infinite poll loop
};

#endif