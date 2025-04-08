/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerSocket.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/08 16:45:28 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/08 17:02:09 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ServerSocket.hpp"

ServerSocket::ServerSocket() :
	m_fd(-1), m_port(0), m_host(127.0.0.1)
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
