/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:34 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/16 19:24:32 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "SocketManager.hpp"
#include <iostream>
#include <unistd.h>
#include <stdexcept>  // std::runtime_error
#include <string>     // include <cstdiocket.hccept
#include <sys/socket.h> //accept()
#include <cstdio>
#include <fcntl.h> 

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

SocketManager& SocketManager::operator=(const SocketManager &src)
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
    std::cout << "Starting poll() loop on " << m_pollfds.size() << " sockets." << std::endl;

    while (true)
    {
		std::cout << "[DEBUG] Polling..." << std::endl;
        int ret = poll(&m_pollfds[0], m_pollfds.size(), -1);
		std::cout << "[DEBUG] poll() returned: " << ret << std::endl;

		for (size_t j = 0; j < m_pollfds.size(); ++j)
		{
			std::cout << "[DEBUG] fd: " << m_pollfds[j].fd
					<< " events: " << m_pollfds[j].events
					<< " revents: " << m_pollfds[j].revents << std::endl;
		}
        if (ret < 0)
        {
            perror("poll() failed");
            break;
        }

        for (size_t i = 0; i < m_pollfds.size(); ++i)
        {
            int fd = m_pollfds[i].fd;

            if (m_pollfds[i].revents & POLLIN)
            {
                // Check if it's a server socket
                if (m_serverFds.count(fd))
                {
                    int client_fd = accept(fd, NULL, NULL);
                    if (client_fd >= 0)
                    {
                        std::cout << "Accepted new client: fd " << client_fd << std::endl;

                        // Set client_fd to non-blocking
                        int flags = fcntl(client_fd, F_GETFL, 0);
                        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

                        // Add to poll list
                        struct pollfd client_poll;
                        client_poll.fd = client_fd;
                        client_poll.events = POLLIN;
                        client_poll.revents = 0;
                        m_pollfds.push_back(client_poll);

                        // Optionally track buffer
                        m_clientBuffers[client_fd] = "";
                    }
                    else
                    {
                        perror("accept() failed");
                    }
                }
                else  // It's a client socket
                {
                    char buffer[1024];
                    int bytes = recv(fd, buffer, sizeof(buffer), 0);

                    if (bytes <= 0)
                    {
                        std::cout << "Client disconnected: fd " << fd << std::endl;
                        close(fd);
                        m_clientBuffers.erase(fd);
                        m_pollfds.erase(m_pollfds.begin() + i);
                        --i; // Adjust index because we erased an element
                    }
                    else
                    {
                        m_clientBuffers[fd].append(buffer, bytes);
                        std::cout << "Received from fd " << fd << ": " << m_clientBuffers[fd] << std::endl;

                        // Send basic 200 OK response
                        std::string response =
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Length: 13\r\n"
                            "Content-Type: text/plain\r\n"
                            "\r\n"
                            "Hello, world!";

                        send(fd, response.c_str(), response.size(), 0);

                        // Close and clean up client connection
                        close(fd);
                        m_clientBuffers.erase(fd);
                        m_pollfds.erase(m_pollfds.begin() + i);
                        --i;
                    }
                }
            }
        }  // end for
    }      // end while
}