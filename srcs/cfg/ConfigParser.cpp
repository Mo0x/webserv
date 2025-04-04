#include "ConfigParser.hpp"
#include "ConfigLexer.hpp"
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <string>

ConfigParser::ConfigParser() : m_filePath("")
{
    parse();
}

ConfigParser::ConfigParser(const std::string &path) : m_filePath(path)
{
    parse();
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

ConfigParser::~ConfigParser()
{
    // Destructeur (aucune libération spécifique n'est nécessaire ici)
}

const std::vector<ServerConfig> &ConfigParser::getServers() const
{
    return (m_servers);
}

void ConfigParser::parse()
{
    ConfigLexer lexer(m_filePath);
    std::vector<Token> tokens = lexer.tokenize();
    size_t current = 0;
    
    while (current < tokens.size() && tokens[current].type != TOKEN_END_OF_FILE)
    {
        if (tokens[current].value == "server")
        {
            current++;  // Consomme le mot-clé 'server'
            if (current >= tokens.size() || tokens[current].value != "{")
            {
                throw std::runtime_error("Attendu '{' après 'server' à la ligne " + std::to_string(tokens[current].line));
            }
            current++;  // Consomme '{'
            ServerConfig server = parseServerBlock(tokens, current);
            m_servers.push_back(server);
        }
        else
        {
            throw std::runtime_error("Jeton inattendu : " + tokens[current].value + " à la ligne " + std::to_string(tokens[current].line));
        }
    }
}

ServerConfig ConfigParser::parseServerBlock(const std::vector<Token>& tokens, size_t &current)
{
    ServerConfig server;
    
    while (current < tokens.size() && tokens[current].value != "}")
    {
        std::string directive = tokens[current].value;
        current++;  // Consomme le jeton de directive
        
        if (directive == "listen")
        {
            if (current >= tokens.size())
            {
                throw std::runtime_error("Fin inattendue des jetons dans la directive 'listen'");
            }
            std::string listenValue = tokens[current].value;
            current++;
            size_t colon = listenValue.find(':');
            if (colon != std::string::npos)
            {
                server.setHost(listenValue.substr(0, colon));
                server.setPort(std::atoi(listenValue.c_str() + colon + 1));
            }
            else
            {
                server.setPort(std::atoi(listenValue.c_str()));
            }
            if (current >= tokens.size() || tokens[current].value != ";")
            {
                throw std::runtime_error("Attendu ';' après la directive 'listen' à la ligne " + std::to_string(tokens[current].line));
            }
            current++;  // Consomme ';'
        }
        else if (directive == "server_name")
        {
            if (current >= tokens.size())
            {
                throw std::runtime_error("Fin inattendue des jetons dans la directive 'server_name'");
            }
            server.setServerName(tokens[current].value);
            current++;
            if (current >= tokens.size() || tokens[current].value != ";")
            {
                throw std::runtime_error("Attendu ';' après la directive 'server_name'");
            }
            current++;  // Consomme ';'
        }
        else if (directive == "error_page")
        {
            if (current + 1 >= tokens.size())
            {
                throw std::runtime_error("Fin inattendue des jetons dans la directive 'error_page'");
            }
            int code = std::atoi(tokens[current].value.c_str());
            current++;
            server.addErrorPage(code, tokens[current].value);
            current++;
            if (current >= tokens.size() || tokens[current].value != ";")
            {
                throw std::runtime_error("Attendu ';' après la directive 'error_page'");
            }
            current++;  // Consomme ';'
        }
        else if (directive == "client_max_body_size")
        {
            if (current >= tokens.size())
            {
                throw std::runtime_error("Fin inattendue des jetons dans la directive 'client_max_body_size'");
            }
            server.setClientMaxBodySize(std::atoi(tokens[current].value.c_str()));
            current++;
            if (current >= tokens.size() || tokens[current].value != ";")
            {
                throw std::runtime_error("Attendu ';' après la directive 'client_max_body_size'");
            }
            current++;  // Consomme ';'
        }
        else
        {
            std::cerr << "Attention : directive inconnue ou mal formée : " << directive
                      << " à la ligne " << tokens[current].line << std::endl;
            // Ignorer les jetons jusqu'au prochain ';'
            while (current < tokens.size() && tokens[current].value != ";")
            {
                current++;
            }
            if (current < tokens.size())
            {
                current++;  // Consomme ';'
            }
        }
    }
    
    if (current >= tokens.size() || tokens[current].value != "}")
    {
        throw std::runtime_error("Manque '}' de fermeture dans le bloc server");
    }
    current++;  // Consomme '}'
    
    return (server);
}
