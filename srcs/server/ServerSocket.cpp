/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerSocket.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/08 16:45:28 by mgovinda          #+#    #+#             */
/*   Updated: 2025/11/24 18:11:07 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ServerSocket.hpp"

#include <stdexcept>      // std::runtime_error
#include <sstream>        // std::ostringstream
#include <string>         // std::string
#include <cstring>        // std::memset, std::strerror
#include <cerrno>         // errno

// Socket and system includes
#include <unistd.h>       // close()
#include <fcntl.h>        // fcntl(), F_GETFL, F_SETFL, O_NONBLOCK
#include <sys/types.h>
#include <sys/socket.h>   // socket(), bind(), listen(), setsockopt()
#include <netdb.h> // getaddrinfo, addrinfo, gai_strerror
#include <iostream>
#include <cstdio>
// Setup: socket() → fcntl() → setsockopt() → getaddrinfo() → bind() → listen()

ServerSocket::ServerSocket() :
	m_fd(-1), m_port(0), m_host("127.0.0.1")
{
    std::cerr << "default used !!!!!!" << std::endl;
	return ;
}

ServerSocket::ServerSocket(const std::string& host, unsigned short port)
	: m_fd(-1), m_port(port), m_host(host)
{
	setup();
}

ServerSocket::ServerSocket(const ServerSocket& other)
	: m_fd(other.m_fd), m_port(other.m_port), m_host(other.m_host)
{
	return ;
}
ServerSocket::~ServerSocket()

{
    std::cout << "[DEBUG] Destroying ServerSocket fd " << m_fd << std::endl;
    if (m_fd != -1)
        close(m_fd);
}
ServerSocket& ServerSocket::operator=(const ServerSocket& other)
{
	if (this != &other)
	{
		m_fd = other.m_fd;
		m_port = other.m_port;
		m_host = other.m_host;
	}
	return *this;
}

int ServerSocket::getFd() const
{
	return m_fd;
}

unsigned short ServerSocket::getPort() const
{
	return m_port;
}

const std::string& ServerSocket::getHost() const
{
	return m_host;
}


void ServerSocket::setup()
{
	m_fd = socket(AF_INET, SOCK_STREAM, 0);
	std::cout << "[DEBUG] socket() returned fd: " << m_fd << std::endl;
	if (m_fd < 0)
	{
		perror("[ERROR] socket() failed");
		throw std::runtime_error("socket() failed");
	}

	if (m_fd < 0)
		throw std::runtime_error("socket() failed: " + std::string(strerror(errno)));

	int opt = 1;
	if (setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		throw std::runtime_error("setsockopt() failed: " + std::string(strerror(errno)));

	setNonBlocking();
	bindSocket();
	listenSocket();
}

void ServerSocket::setNonBlocking()
{
	if (fcntl(m_fd, F_SETFL, O_NONBLOCK) < 0)
		throw std::runtime_error("fcntl(F_SETFL) failed: " + std::string(strerror(errno)));
}


void ServerSocket::bindSocket()
{
    struct addrinfo hints;
    struct addrinfo* res = NULL;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4 only
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    std::ostringstream oss;
    oss << m_port;
    std::string portStr = oss.str();

    std::cout << "[DEBUG] Trying to bind to " << m_host << ":" << portStr << std::endl;

    int ret = getaddrinfo(m_host.c_str(), portStr.c_str(), &hints, &res);
    if (ret != 0)
    {
        std::cerr << "[ERROR] getaddrinfo failed: " << gai_strerror(ret) << std::endl;
        throw std::runtime_error("getaddrinfo() failed");
    }

    if (bind(m_fd, res->ai_addr, res->ai_addrlen) < 0)
    {
        perror("[ERROR] bind() failed");
        freeaddrinfo(res);
        std::ostringstream err;
        err << "bind() failed on " << m_host << ":" << m_port << " - " << strerror(errno);
        throw std::runtime_error(err.str());
    }
    else
    {
        std::cout << "[DEBUG] bind() success on fd " << m_fd << std::endl;
    }

    freeaddrinfo(res);
}

void ServerSocket::listenSocket()
{
    if (listen(m_fd, 128) < 0)
    {
        perror("[ERROR] listen() failed");
        throw std::runtime_error("listen() failed: " + std::string(strerror(errno)));
    }
    else
    {
        std::cout << "[DEBUG] listen() success on fd " << m_fd << std::endl;
    }
}
bool ServerSocket::isValid() const
{
	return m_fd != -1;
}

