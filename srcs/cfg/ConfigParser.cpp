#include "ConfigParser.hpp"
#include "ConfigLexer.hpp"
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <sstream>

ConfigParser::ConfigParser() : m_filePath("")
{
    parse();
}

ConfigParser::ConfigParser(const std::string &path) : m_filePath(path)
{
    this->parse();
}

ConfigParser::ConfigParser(const ConfigParser &src)
{
    *this = src;
}

ConfigParser &ConfigParser::operator=(const ConfigParser &src)
{
    if (this != &src)
    {
        this->m_filePath = src.m_filePath;
        this->m_servers = src.m_servers;
    }
    return (*this);
}

ConfigParser::~ConfigParser() {}

const std::vector<ServerConfig> &ConfigParser::getServers() const
{
    return this->m_servers;
}

void ConfigParser::parse()
{
    std::ifstream file(this->m_filePath.c_str());
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open configuration file.");
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string configContent = buffer.str();
    file.close();

    ConfigLexer lexer(this->m_filePath);
    std::vector<Token> tokens = lexer.tokenize();

    size_t current = 0;
    while (current < tokens.size())
    {
        if (tokens[current].value == "server")
        {
            current++;
            ServerConfig serverConfig = parseServerBlock(tokens, current);
            this->m_servers.push_back(serverConfig);
        }
        else
        {
            current++;
        }
    }
}

// Modified parseServerBlock() method
ServerConfig ConfigParser::parseServerBlock(const std::vector<Token>& tokens, size_t &current)
{
    ServerConfig server;
    if (tokens[current].value != "{")
    {
        throw std::runtime_error("Expected '{' at beginning of server block.");
    }
    current++;

    while (current < tokens.size() && tokens[current].value != "}")
    {
        if (tokens[current].value == "host")
        {
            current++;
            if (current < tokens.size())
            {
                server.setHost(tokens[current].value);
                current++;
            }
            if (tokens[current].value == ";")
            {
                current++;
            }
        }
        else if (tokens[current].value == "port" || tokens[current].value == "listen")
        {
            current++;
            if (current < tokens.size())
            {
                server.setPort(atoi(tokens[current].value.c_str()));
                current++;
            }
            if (tokens[current].value == ";")
            {
                current++;
            }
        }
        else if (tokens[current].value == "server_name")
        {
            current++;
            if (current < tokens.size())
            {
                server.setServerName(tokens[current].value);
                current++;
            }
            if (tokens[current].value == ";")
            {
                current++;
            }
        }
        else if (tokens[current].value == "client_max_body_size")
        {
            current++;
            if (current < tokens.size())
            {
                server.setClientMaxBodySize(static_cast<size_t>(strtoul(tokens[current].value.c_str(), NULL, 10)));
                current++;
            }
            if (tokens[current].value == ";")
            {
                current++;
            }
        }
        else if (tokens[current].value == "location")
        {
            current++;
            // NEW: Parse location block and add as a route configuration
            LocationConfig location = parseLocationBlock(tokens, current);
            server.addLocation(location);
        }
        else
        {
            current++;
        }
    }
    if (current < tokens.size() && tokens[current].value == "}")
    {
        current++;
    }
    else
    {
        throw std::runtime_error("Expected '}' at end of server block.");
    }
    return server;
}

// NEW: Added parseLocationBlock() method to support route configuration
LocationConfig ConfigParser::parseLocationBlock(const std::vector<Token>& tokens, size_t &current)
{
    LocationConfig loc;
    // Next token is the location path (e.g., "/images")
    if (current < tokens.size())
    {
        loc.path = tokens[current].value;
        current++;
    }
    else
    {
        throw std::runtime_error("Expected location path.");
    }

    // Expect opening '{' for location block
    if (tokens[current].value != "{")
    {
        throw std::runtime_error("Expected '{' at beginning of location block.");
    }
    current++;

    // Process directives inside the location block until "}" is reached
    while (current < tokens.size() && tokens[current].value != "}")
    {
        if (tokens[current].value == "root")
        {
            ++current;
            if (current < tokens.size())
            {
                loc.root = tokens[current].value;
                ++current;
            }
            if (current < tokens.size() && tokens[current].value == ";")
            {
                ++current;
            }
            else if (tokens[current].value == "index")
            {
                ++current;
                if (current < tokens.size())
                {
                    loc.index = tokens[current].value;
                    ++current;
                }
                if (current < tokens.size() && tokens[current].value == ";")
                    ++current;
            }
        }
        else if (tokens[current].value == "allowedMethods")
        {
            current++;
            // Continue reading allowed methods until semicolon is encountered.
            while (current < tokens.size() && tokens[current].value != ";")
            {
                loc.allowedMethods.push_back(tokens[current].value);
                current++;
            }
            if (tokens[current].value == ";")
            {
                current++;
            }
        }
        else if (tokens[current].value == "autoindex")
        {
            current++;
            if (current < tokens.size())
            {
                if (tokens[current].value == "on")
                {
                    loc.autoindex = true;
                }
                else
                {
                    loc.autoindex = false;
                }
                current++;
            }
            if (tokens[current].value == ";")
            {
                current++;
            }
        }
        else if (tokens[current].value == "redirect")
        {
            current++;
            if (current < tokens.size())
            {
                loc.redirect = tokens[current].value;
                current++;
            }
            if (tokens[current].value == ";")
            {
                current++;
            }
        }
        else if (tokens[current].value == "uploadPath")
        {
            current++;
            if (current < tokens.size())
            {
                loc.uploadPath = tokens[current].value;
                current++;
            }
            if (tokens[current].value == ";")
            {
                current++;
            }
        }
        else if (tokens[current].value == "cgiPath")
        {
            current++;
            if (current < tokens.size())
            {
                loc.cgiPath = tokens[current].value;
                current++;
            }
            if (tokens[current].value == ";")
            {
                current++;
            }
        }
        else if (tokens[current].value == "cgiExtension")
        {
            current++;
            if (current < tokens.size())
            {
                loc.cgiExtension = tokens[current].value;
                current++;
            }
            if (tokens[current].value == ";")
            {
                current++;
            }
        }
    /*    else if (tokens[current].value == "error_page")
        {
            current++;
            if (current + 1 < tokens.size())
            {
                int code = atoi(tokens[current++].value.c_str());
                std::string path = tokens[current++].value;
                server.addErrorPage(code, path);
            }
            if (tokens[current].value == ";") current++;
        }
            */
        else
        {
            // If an unrecognized directive is found, skip it.
            current++;
        }
    }
    if (current < tokens.size() && tokens[current].value == "}")
    {
        current++;
    }
    else
    {
        throw std::runtime_error("Expected '}' at end of location block.");
    }
    return loc;
}