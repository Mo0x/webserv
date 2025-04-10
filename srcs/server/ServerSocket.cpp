/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerSocket.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/08 16:45:28 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/10 17:10:35 by mgovinda         ###   ########.fr       */
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
#include <netinet/in.h>   // sockaddr_in, htons()
#include <arpa/inet.h>    // inet_addr()

ServerSocket::ServerSocket() :
	m_fd(-1), m_port(0), m_host("127.0.0.1")
{
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
	int flags = fcntl(m_fd, F_GETFL, 0);
	if (flags < 0)
		throw std::runtime_error("fcntl(F_GETFL) failed: " + std::string(strerror(errno)));

	if (fcntl(m_fd, F_SETFL, flags | O_NONBLOCK) < 0)
		throw std::runtime_error("fcntl(F_SETFL) failed: " + std::string(strerror(errno)));
}

void ServerSocket::bindSocket()
{
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(m_port);
	addr.sin_addr.s_addr = inet_addr(m_host.c_str());

	if (bind(m_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		std::ostringstream oss;
		oss << "bind() failed on " << m_host << ":" << m_port << " - " << strerror(errno);
		throw std::runtime_error(oss.str());
	}
}


void ServerSocket::listenSocket()
{
	if (listen(m_fd, 128) < 0)
		throw std::runtime_error("listen() failed: " + std::string(strerror(errno)));
}
