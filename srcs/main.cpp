/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/20 16:04:39 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/16 19:05:12 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <csignal>
#include <unistd.h>
#include <vector>
#include <map>
#include <string>
#include <cstdlib>

#include "ConfigParser.hpp"
#include "ServerSocket.hpp"
#include "MultipartParser.hpp"
#include "FileUploadHandler.hpp"
#include "SocketManager.hpp"

volatile bool g_running = true;

void handleSignal(int)
{
    g_running = false;
}

// Test du parsing de config et du routing
void testParsedConfig(const std::vector<ServerConfig>& servers)
{
    std::string requestMethod = "GET";
    std::string requestUri    = "/images/";

    if (servers.empty())
    {
        std::cerr << "No server configuration found." << std::endl;
        return;
    }

    const ServerConfig& server = servers[0];
    const std::vector<LocationConfig>& locs = server.getLocations();

    const LocationConfig* best = NULL;
    size_t longest = 0;
    for (size_t i = 0; i < locs.size(); ++i)
    {
        if (requestUri.find(locs[i].path) == 0 && locs[i].path.size() > longest)
        {
            longest = locs[i].path.size();
            best    = &locs[i];
        }
    }

    if (!best)
    {
        std::cout << "404 Not Found: " << requestUri << std::endl;
        return;
    }

    // Méthode autorisée ?
    bool ok = false;
    for (size_t i = 0; i < best->allowedMethods.size(); ++i)
        if (best->allowedMethods[i] == requestMethod)
            ok = true;

    if (!ok)
    {
        std::cout << "405 Method Not Allowed on " << best->path << std::endl;
    }
    else if (!best->redirect.empty())
    {
        std::cout << "Redirect to " << best->redirect << std::endl;
    }
    else
    {
        // C++98 : on n’utilise pas back()
        if (!requestUri.empty() && requestUri[requestUri.size() - 1] == '/')
        {
            if (!best->index.empty())
                std::cout << "Index: " << best->root << "/" << best->index << std::endl;
            else if (best->autoindex)
                std::cout << "Autoindex on " << best->root << std::endl;
            else
                std::cout << "403/404: no index & autoindex off" << std::endl;
        }
        else
        {
            std::cout << "Serving: " << best->root << requestUri << std::endl;
        }
    }
}

// Test unitaire multipart/form-data + upload
void testMultipart()
{
    std::string boundary    = "MyBoundary";
    std::string contentType = "multipart/form-data; boundary=" + boundary;

    std::string body;
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"field1\"\r\n\r\n";
    body += "value1\r\n";
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"file1\"; filename=\"test.txt\"\r\n";
    body += "Content-Type: text/plain\r\n\r\n";
    body += "Dummy file content.\r\n";
    body += "--" + boundary + "--\r\n";

    try
    {
        MultipartParser parser(body, contentType);
        std::vector<MultipartPart> parts = parser.parse();

        std::cout << "[Multipart] parts = " << parts.size() << std::endl;
        for (size_t i = 0; i < parts.size(); ++i)
        {
            std::cout << "Part " << i
                      << " isFile=" << (parts[i].isFile ? "yes" : "no")
                      << ", filename=" << parts[i].filename
                      << ", content='" << parts[i].content << "'" << std::endl;

            if (parts[i].isFile)
            {
                std::string saved = FileUploadHandler::saveUploadedFile(
                    parts[i], "/tmp/uploads", 1048576);
                std::cout << " → saved to: " << saved << std::endl;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Multipart] Error: " << e.what() << std::endl;
    }
}

int main(int argc, char** argv)
{
    std::string cfg = "config.conf";
    if (argc == 2)
        cfg = argv[1];
    else if (argc > 2)
    {
        std::cerr << "Usage: " << argv[0] << " [configuration file]" << std::endl;
        return 1;
    }

    // Création du dossier d’upload si besoin
    system("mkdir -p /tmp/uploads && chmod 755 /tmp/uploads");

    try
    {
        std::signal(SIGINT, handleSignal);

        ConfigParser parser(cfg);
        std::vector<ServerConfig> servers = parser.getServers();

        std::cout << "=== Config Tests ===\n";
        testParsedConfig(servers);

        std::cout << "\n=== Multipart Tests ===\n";
        testMultipart();

        std::cout << "\n=== Starting server ===\n";
        SocketManager sm;
        // ajoutez ici autant de serveurs que vous voulez à partir de la config
        sm.addServer(servers[0].getHost(), servers[0].getPort());
        sm.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 2;
    }

    return 0;
}