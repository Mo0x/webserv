/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/20 16:04:39 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/08 15:55:01 by mgovinda         ###   ########.fr       */
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

 #include <iostream>
#include "ConfigParser.hpp"

int main(int argc, char **argv)
{
	std::string cfg = "default.conf"; // <- missing semicolon
	if (argc == 2)
		cfg = argv[1];
	else if (argc > 2) // <- should be argc, not argv
	{
		std::cerr << "Usage: ./webserv [configuration file]" << std::endl;
		return (1);
	}
	try	
	{
		ConfigParser parser(cfg);
		std::vector<ServerConfig> servers = parser.getServers();

		// 🔍 Let's print out what was parsed
		for (size_t i = 0; i < servers.size(); ++i)
		{
			std::cout << "--- Server Block " << i << " ---" << std::endl;
			std::cout << "Host: " << servers[i].getHost() << std::endl;
			std::cout << "Port: " << servers[i].getPort() << std::endl;
			std::cout << "Server Name: " << servers[i].getServerName() << std::endl;
			std::cout << "Client Max Body Size: " << servers[i].getClientMaxBodySize() << std::endl;

			const std::map<int, std::string>& errorPages = servers[i].getErrorPages();
			for (std::map<int, std::string>::const_iterator it = errorPages.begin(); it != errorPages.end(); ++it)
			{
				std::cout << "Error " << it->first << ": " << it->second << std::endl;
			}
		}
	

        // ------------------ Simulated Request Handling ------------------

        // For this example, we simulate processing a single HTTP request.
        // In a real scenario, you would accept connections on a socket.
        // Here, we hardcode a sample request.
        std::string requestMethod = "GET";  // Sample HTTP method (can be GET, POST, etc.)
        std::string requestUri = "/images/";  // Sample Request URI

        // For simplicity, we select the first server configuration.
        if (servers.empty())
        {
            std::cerr << "No server configuration found." << std::endl;
            return (1);
        }
        ServerConfig server = servers[0];

        // Route matching: iterate through location blocks to find the best match.
        const std::vector<LocationConfig>& locations = server.getLocations();
        const LocationConfig* matchedLocation = NULL;
        size_t longestMatch = 0;

        for (size_t i = 0; i < locations.size(); ++i)
        {
            // If the request URI starts with the location path,
            // check if it is the longest match found so far.
            if (requestUri.find(locations[i].path) == 0)
            {
                size_t len = locations[i].path.length();
                if (len > longestMatch)
                {
                    longestMatch = len;
                    matchedLocation = &locations[i];
                }
            }
        }

        if (matchedLocation == NULL)
        {
            std::cout << "404 Not Found: No matching route for " << requestUri << std::endl;
        }
        else
        {
            // Check if the HTTP method is allowed for the matched location.
            bool methodAllowed = false;
            for (size_t i = 0; i < matchedLocation->allowedMethods.size(); ++i)
            {
                if (matchedLocation->allowedMethods[i] == requestMethod)
                {
                    methodAllowed = true;
                    break;
                }
            }

            if (!methodAllowed)
            {
                std::cout << "405 Method Not Allowed: " << requestMethod
                          << " is not allowed on " << matchedLocation->path << std::endl;
            }
            else
            {
                // Check if a redirect is specified for the location.
                if (!matchedLocation->redirect.empty())
                {
                    std::cout << "Redirect (3xx): " << matchedLocation->redirect << std::endl;
                }
                else
                {
                    // Handle directory vs. file requests:
                    // If the request URI ends with '/', it's a directory.
                    if (requestUri[requestUri.length() - 1] == '/')
                    {
                        // Attempt to serve the default index file for the directory.
                        if (!matchedLocation->index.empty())
                        {
                            std::string filePath = matchedLocation->root + "/" + matchedLocation->index;
                            std::cout << "Serving index file: " << filePath << std::endl;
                        }
                        else if (matchedLocation->autoindex)
                        {
                            // Generate and serve directory listing.
                            std::cout << "Serving autoindex for directory: " << matchedLocation->root << std::endl;
                        }
                        else
                        {
                            std::cout << "403 Forbidden/404 Not Found: No index file and autoindex is disabled." << std::endl;
                        }
                    }
                    else
                    {
                        // For a file request (non-directory), simulate serving a static file.
                        std::string filePath = matchedLocation->root + requestUri;
                        // In a real server, check file existence and permissions.
                        std::cout << "Serving static file: " << filePath << std::endl;
                    }
                }
            }
        }
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error parsing config file: " << e.what() << std::endl;
		return (1);
	}
	return (0);
}
