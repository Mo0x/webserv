#include "ConfigParser.hpp"
#include "ConfigLexer.hpp"
#include "Config.hpp"
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <sstream>

/*TODO change from the Serverconfig class (.hpp) to the struct from Config.hpp
    JAI FAIS UN CHANGEMENT POUR errorpage 404 qui etait std::stirng et mtn cest un std::map<int, std::string> error_pages
    si ya des bug la dessus cest surement que jai pas gerer le refactoring
*/

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
                std::ostringstream oss;
                oss << "Attendu '{' après 'server' à la ligne " << tokens[current].line;
                throw std::runtime_error(oss.str());
            }
            current++;  // Consomme '{'
            ServerConfig server = parseServerBlock(tokens, current);
            m_servers.push_back(server);
        }
        else
        {
            std::ostringstream oss;
            oss << "Jeton inattendu : " << tokens[current].value << " à la ligne " << tokens[current].line;
            throw std::runtime_error(oss.str());
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
                server.host = listenValue.substr(0, colon);
                server.port = std::atoi(listenValue.c_str() + colon + 1);
            }
            else
            {
                server.port = std::atoi(listenValue.c_str());
            }
            if (current >= tokens.size() || tokens[current].value != ";")
            {
                std::ostringstream oss;
                oss << "Attendu ';' après la directive 'listen' à la ligne " << tokens[current].line;
                throw std::runtime_error(oss.str());
            }
            current++;  // Consomme ';'
        }
        else if (directive == "server_name")
        {
            if (current >= tokens.size())
            {
                throw std::runtime_error("Fin inattendue des jetons dans la directive 'server_name'");
            }
            server.server_name = tokens[current].value;
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
            std::string path = tokens[current].value;
            current++;
            if (current >= tokens.size() || tokens[current].value != ";")
            {
                std::ostringstream oss;
                oss << "Attendu ';' après la directive 'error_page' à la ligne " << tokens[current - 1].line;
                throw std::runtime_error(oss.str());
            }
            server.error_pages[code] = path;
            current++;  // consume ';'
        }
        else if (directive == "client_max_body_size")
        {
            if (current >= tokens.size())
            {
                throw std::runtime_error("Fin inattendue des jetons dans la directive 'client_max_body_size'");
            }
            server.client_max_body_size = std::atoi(tokens[current].value.c_str());
            current++;
            if (current >= tokens.size() || tokens[current].value != ";")
            {
                throw std::runtime_error("Attendu ';' après la directive 'client_max_body_size'");
            }
            current++;  // Consomme ';'
        }
        /* here is just added the parse Location bloc that call parseLocationBlock()*/
        else if (directive == "location")
        {
            if (current >= tokens.size())
                throw std::runtime_error("Expected path after 'location'");
            RouteConfig route;
            route.path = tokens[current++].value;

            if (current >= tokens.size() || tokens[current++].value != "{")
                throw std::runtime_error("Expected '{' after location path");

            route = parseLocationBlock(tokens, current);
            server.routes.push_back(route);
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

RouteConfig ConfigParser::parseLocationBlock(const std::vector <Token> tokens, size_t &current)
{
    RouteConfig ret;
    
    while (current < tokens.size() && tokens[current].value != "}")
    {
        std::string directive = tokens[current++].value;
        if (directive == "allowed_methods")
        {
            while (tokens[current].value != ";")
            {
                ret.allowed_methods.insert(tokens[current++].value);
                if (current >= tokens.size())
                    throw std::runtime_error("Unexpected end of allowed_methods");
            }
            current ++;
        }
        else if (directive == "root")
        {
            ret.root = tokens[current++].value;
                if (current >= tokens.size())
                    throw std::runtime_error("Unexpected ';' after root");
        }
        else if (directive == "index")
        {
            ret.root = tokens[current++].value;
                if (current >= tokens.size())
                    throw std::runtime_error("Unexpected ';' after index");
        }
        else if (directive == "upload_path")
        {
            ret.root = tokens[current++].value;
                if (current >= tokens.size())
                    throw std::runtime_error("Unexpected ';' after upload_path");
        }
        else if (directive == "autoindex")
        {
            ret.root = tokens[current++].value;
                if (current >= tokens.size())
                    throw std::runtime_error("Unexpected ';' after autoindex");
        }
        else if (directive == "cgi_extension")
        {
            ret.root = tokens[current++].value;
                if (current >= tokens.size())
                    throw std::runtime_error("Unexpected ';' after cgi_extension");
        }
        else if (directive == "max_body_size")
        {
            ret.root = tokens[current++].value;
                if (current >= tokens.size())
                    throw std::runtime_error("Unexpected ';' after max_body_size");
        }
        else if (directive == "redirect")
        {
            ret.root = tokens[current++].value;
                if (current >= tokens.size())
                    throw std::runtime_error("Unexpected ';' after redirect");
        }
        else
        {
            std::cerr << "Unkwon directive in location: " << directive << std::endl;
            while (tokens[current].value != ";" && tokens[current].value != "}")
                current ++;
            if (tokens[current].value == ";")
                current++;
        }
    }
    if (tokens[current].value != "}")
        throw std::runtime_error("Expected } to close location block");
    current++;

    return ret;
}