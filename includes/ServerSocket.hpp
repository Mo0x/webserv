/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerSocket.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/08 16:35:57 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/16 19:19:41 by mgovinda         ###   ########.fr       */
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

	
	void setNonBlocking();
	void bindSocket();
	void listenSocket();
	ServerSocket &operator=(const ServerSocket &src);
	ServerSocket(const ServerSocket &src);
	
	public:
	ServerSocket();
	ServerSocket(const std::string &host, unsigned short port);
	~ServerSocket();

	//get
	int	getFd() const;
	unsigned short	getPort() const;
	const std::string &getHost() const;

	//set
	void setup(); //does socket(), bind(), listen();
	bool isValid() const;
};

#endif