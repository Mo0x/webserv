/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:34 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/13 19:22:46 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "SocketManager.hpp"
#include <iostream>
#include <unistd.h>
#include <stdexcept>  // std::runtime_error
#include <string>     // std::string
#include <sys/socket.h> // accept()
SocketManager::SocketManager()
{
	return ;
}

SocketManager::SocketManager(const SocketManager &src) :
	m_servers(src.m_servers),
	m_pollfds(src.m_pollfds)
{
	return ;
}

SocketManager &SocketManager::operator=(const SocketManager &src)
{
	if (this != &src)
	{
		m_servers = src.m_servers;
		m_pollfds = src.m_pollfds;
	}
	return *this;
}

SocketManager::~SocketManager()
{
	return ;
}

void SocketManager::addServer(const std::string &host, unsigned short port)
{
	ServerSocket server(host, port);
	m_servers.push_back(server);
}

void SocketManager::initPoll()
{
	m_pollfds.clear();
	for (size_t i = 0; i < m_servers.size(); i++)
	{
		struct pollfd pfd;
		pfd.fd = m_servers[i].getFd();
		pfd.events = POLLIN;
		pfd.revents = 0;
		m_pollfds.push_back(pfd);
	}
}

void SocketManager::run()
{
	initPoll();
	std::cout << "Starting poll() loop on " << m_pollfds.size() << " sockets." << std::endl;
	while (1)
	{
		int ret = poll(&m_pollfds[0], m_pollfds.size(), -1);
		if (ret < 0)
		{
			perror("poll() failed");
			break ;
		}
		for (size_t i = 0; i < m_pollfds.size(); ++i)
		{
			if (m_pollfds[i].revents & POLLIN)
			{
				int client_fd = accept(m_pollfds[i].fd, NULL, NULL);
				if (client_fd >= 0)
				{
					std::cout << "Accepted new client: fd " << client_fd << std::endl;
					//here set non blocking? + add to poll
				}
				else
				{
					perror("accept() failed");
				}
			}
		}
	}
}