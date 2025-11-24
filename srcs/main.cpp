/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/20 16:04:39 by mgovinda          #+#    #+#             */
/*   Updated: 2025/11/24 18:51:04 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

// TEST MAIN for build error reponse when multiple configs FOLLOWING :

#include <iostream>
#include "ConfigParser.hpp"
#include "SocketManager.hpp"
#include <csignal>

volatile sig_atomic_t g_stop = 0; 

extern "C" void handleSigint(int) //here we force g_stop to use C convention as signal expect a function pointer
{
	g_stop = 1;
	std::cerr << "Signal g_stop to 1" << std::endl;
}

int main(int argc, char** argv)
{
	std::signal(SIGINT, handleSigint);
	try 
	{
		std::string configFile = "config.conf";  // default fallback
		if (argc > 1)
			configFile = argv[1];

		ConfigParser parser(configFile);
		std::vector<ServerConfig> servers = parser.getServers();
		if (servers.empty())
		{
			std::cerr << "No server blocks defined!" << std::endl;
			return 1;
		}

		std::cout << "Starting WebServer now..." << std::endl;

		SocketManager sm;
		sm.setServers(servers);

		for (size_t i = 0; i < servers.size(); ++i)
			sm.addServer(servers[i].host, servers[i].port);
		sm.run();
	}
	catch (const std::exception & e)
	{
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
