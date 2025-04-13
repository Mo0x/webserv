#include <iostream>
#include <csignal>
#include <unistd.h>
#include "ConfigParser.hpp"
#include "ServerSocket.hpp"

volatile bool g_running = true;

void handleSignal(int)
{
    g_running = false;
}

void testParsedConfig(const std::vector<ServerConfig>& servers)
{
    std::string requestMethod = "GET";
    std::string requestUri = "/images/";

    if (servers.empty())
    {
        std::cerr << "No server configuration found." << std::endl;
        return;
    }

    ServerConfig server = servers[0];
    const std::vector<LocationConfig>& locations = server.getLocations();
    const LocationConfig* matchedLocation = NULL;
    size_t longestMatch = 0;

    for (size_t i = 0; i < locations.size(); ++i)
    {
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
        else if (!matchedLocation->redirect.empty())
        {
            std::cout << "Redirect (3xx): " << matchedLocation->redirect << std::endl;
        }
        else
        {
            if (requestUri.back() == '/')
            {
                if (!matchedLocation->index.empty())
                {
                    std::string filePath = matchedLocation->root + "/" + matchedLocation->index;
                    std::cout << "Serving index file: " << filePath << std::endl;
                }
                else if (matchedLocation->autoindex)
                {
                    std::cout << "Serving autoindex for directory: " << matchedLocation->root << std::endl;
                }
                else
                {
                    std::cout << "403 Forbidden/404 Not Found: No index file and autoindex is disabled." << std::endl;
                }
            }
            else
            {
                std::string filePath = matchedLocation->root + requestUri;
                std::cout << "Serving static file: " << filePath << std::endl;
            }
        }
    }
}

int main(int argc, char **argv)
{
    std::string cfg = "default.conf";
    if (argc == 2)
        cfg = argv[1];
    else if (argc > 2)
    {
        std::cerr << "Usage: ./webserv [configuration file]" << std::endl;
        return (1);
    }

    try
    {
        std::signal(SIGINT, handleSignal);

        ConfigParser parser(cfg);
        std::vector<ServerConfig> servers = parser.getServers();

        // DEBUG block for testing config parsing + routing
        std::cout << "Running config tests...\n";
        testParsedConfig(servers);

        ServerConfig config = servers[0];
        ServerSocket server(config.getHost(), config.getPort());

        std::cout << "Server listening on " << server.getHost() << ":" << server.getPort() << "\n";
        std::cout << "Press Ctrl+C to stop.\n";

        while (g_running)
            pause();

        std::cout << "Shutting down.\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
