/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/01 12:47:21 by mgovinda          #+#    #+#             */
/*   Updated: 2025/05/24 19:58:29 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include "ServerConfig.hpp"
#include "SocketManager.hpp"
#include "request_response_struct.hpp" // for Response

// Forward declare parser
#include "RequestParser.hpp"

class Server
{
	private:
    ServerConfig   _config;
    SocketManager  _sockMgr;

	public:
	Server();
	Server(const Server &src);
	explicit Server(const ServerConfig& config);
	Server &operator=(const Server &src);
	~Server();
};

#endif
