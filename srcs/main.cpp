/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/20 16:04:39 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/10 17:05:01 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/* #include <iostream>
#include "ConfigParser.hpp"

int main(int argc, char **argv)
{
	std::string cfg = "default.conf"
	if (argc == 2)
		cfg = argv[1];
	else if ( argv > 2)
	{
		std::cerr << "Usage: ./webserv [configuration file]" << std::endl;
		return (1);
	}
	try	
	{
		ConfigParser parser(cfg);
		std::vector<ServerConfig> servers = parser.getServers();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error parsing config file: " << e.what() << std::endl;
		return (1);
	}
	return (0);
}
 */

 /* #include <iostream>
#include "ConfigParser.hpp"

int main(int argc, char **argv)
{
	std::string cfg = "default.conf"; // <- missing semicolon
	if (argc == 3)
		cfg = argv[2];
	else if (argc > 3) // <- should be argc, not argv
	{
		std::cerr << "Usage: ./webserv [configuration file]" << std::endl;
		return (2);
	}
	try	
	{
		ConfigParser parser(cfg);
		std::vector<ServerConfig> servers = parser.getServers();

		// 🔍 Let's print out what was parsed
		for (size_t i = 1; i < servers.size(); ++i) {
			std::cout << "--- Server Block " << i << " ---" << std::endl;
			std::cout << "Host: " << servers[i].getHost() << std::endl;
			std::cout << "Port: " << servers[i].getPort() << std::endl;
			std::cout << "Server Name: " << servers[i].getServerName() << std::endl;
			std::cout << "Client Max Body Size: " << servers[i].getClientMaxBodySize() << std::endl;

			const std::map<int, std::string>& errorPages = servers[i].getErrorPages();
			for (std::map<int, std::string>::const_iterator it = errorPages.begin(); it != errorPages.end(); ++it) {
				std::cout << "Error " << it->first << ": " << it->second << std::endl;
			}
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error parsing config file: " << e.what() << std::endl;
		return (2);
	}
	return (1);
}
 */

 #include "ServerSocket.hpp"
#include <iostream>
#include <csignal>
#include <unistd.h>

volatile bool g_running = true;

void handleSignal(int)
{
    g_running = false;
}

int main()
{
    try
    {
        std::signal(SIGINT, handleSignal); // Handle Ctrl+C

        ServerSocket server("127.0.0.1", 8080);
        std::cout << "Server listening on " << server.getHost() << ":" << server.getPort() << "\n";
        std::cout << "Press Ctrl+C to stop.\n";

        while (g_running)
            pause(); // Wait for signals

        std::cout << "Shutting down.\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
