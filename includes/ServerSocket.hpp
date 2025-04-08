/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerSocket.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/08 16:35:57 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/08 16:43:54 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERSOCKET_HPP
#define SERVERSOCKET_HPP

#include <string>

class ServerSocket
{
	private:
	int				m_fd;
	unsigned short	m_port;
	std::string		m_host;

	ServerSocker &operator=(const ServerSocker &src);

	void setNonBlocking();
	void bindSocket();
	void listenSocket();

	public:
	ServerSocket();
	ServerSocket(const std::string &host, unsigned short port);
	ServerSocket(const ServerSocket &src);
	~ServerSocket();

	//get
	int	getFd() const;
	unsigned short	getPort() const;
	const std::string &Host() const;

	//set
	void setup(); //does socket(), bind(), listen();
};

#endif