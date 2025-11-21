/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/20 16:04:39 by mgovinda          #+#    #+#             */
/*   Updated: 2025/11/21 16:12:06 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

// TEST MAIN for build error reponse when multiple configs FOLLOWING :

#include <iostream>
#include "ConfigParser.hpp"
#include "SocketManager.hpp"

int main(int argc, char** argv)
{
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
