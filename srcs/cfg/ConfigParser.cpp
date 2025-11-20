#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "Config.hpp"
#include "ConfigLexer.hpp"
#include "ConfigParser.hpp"
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
    return m_servers;
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
/* ServerConfig ConfigParser::parseServerBlock(const std::vector<Token>& tokens, size_t &current)
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
        // here is just added the parse Location bloc that call parseLocationBlock()
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
        else if (directive == "root") 
        {
            if (current >= tokens.size())
                throw std::runtime_error("Unexpected end of tokens in 'root'");
        server.root = tokens[current++].value;
        if (tokens[current++].value != ";")
        throw std::runtime_error("Expected ';' after root");
        }
        else if (directive == "index")
        {
            if (current >= tokens.size())
                throw std::runtime_error("Unexpected end of tokens in 'index'");
            server.index = tokens[current++].value;
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after index");
        }
        else 
        {
            std::cerr << "Unknown directive in location: " << directive << std::endl;

            // Skip until ';' or '}'
            while (current < tokens.size() && 
                tokens[current].value != ";" && 
                tokens[current].value != "}")
            {
                current++;
            }
            if (current < tokens.size() && tokens[current].value == ";")
                current++; // skip semicolon
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
    current++;  // Consomme '}'
    return (server);
} */

// Parse fixed for build response error test:

ServerConfig ConfigParser::parseServerBlock(const std::vector<Token>& tokens, size_t &current)
{
	ServerConfig server;

	if (tokens[current].value != "{")
		throw std::runtime_error("Expected '{' at beginning of server block.");
	current++;

	while (current < tokens.size() && tokens[current].value != "}")
	{
        std::cout << "[DEBUG] Token: '" << tokens[current].value << "'" << std::endl;
        /*ADDED COMMENT INLINE ONLY as : #comnmented line*/
        std::string raw = tokens[current].value;
        if (raw.empty() || raw[0] == '#')
        {
            ++current;
            continue;
        }
        std::string directive = raw; 
        current++;

		if (directive == "listen")
		{
			if (current >= tokens.size()) throw std::runtime_error("Missing value for 'listen'");
			std::string listenValue = tokens[current++].value;

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

			if (tokens[current++].value != ";")
				throw std::runtime_error("Expected ';' after 'listen'");
		}
		else if (directive == "server_name")
		{
			if (current >= tokens.size()) throw std::runtime_error("Missing value for 'server_name'");
			server.server_name = tokens[current++].value;
			if (tokens[current++].value != ";")
				throw std::runtime_error("Expected ';' after 'server_name'");
		}
		else if (directive == "root")
		{
			if (current >= tokens.size()) throw std::runtime_error("Missing value for 'root'");
			server.root = tokens[current++].value;
			if (tokens[current++].value != ";")
				throw std::runtime_error("Expected ';' after 'root'");
		}
		else if (directive == "index")
		{
			if (current >= tokens.size()) throw std::runtime_error("Missing value for 'index'");
			server.index = tokens[current++].value;
			if (tokens[current++].value != ";")
				throw std::runtime_error("Expected ';' after 'index'");
		}
		else if (directive == "error_page")
		{
			if (current + 1 >= tokens.size()) throw std::runtime_error("Invalid 'error_page' directive");

			int code = std::atoi(tokens[current++].value.c_str());
			std::string path = tokens[current++].value;

			if (tokens[current++].value != ";")
				throw std::runtime_error("Expected ';' after 'error_page'");

			server.error_pages[code] = path;
		}
		else if (directive == "client_max_body_size")
		{
			if (current >= tokens.size()) throw std::runtime_error("Missing value for 'client_max_body_size'");
			server.client_max_body_size = static_cast<size_t>(strtoul(tokens[current++].value.c_str(), NULL, 10));
			if (tokens[current++].value != ";")
				throw std::runtime_error("Expected ';' after 'client_max_body_size'");
		}

		else if (directive == "location")
        {
            if (current >= tokens.size()) throw std::runtime_error("Expected path after 'location'");
            std::string locPath = tokens[current++].value;

            if (current >= tokens.size() || tokens[current++].value != "{")
                throw std::runtime_error("Expected '{' after location path");

            RouteConfig route = parseLocationBlock(tokens, current);
	        route.path = locPath;  // ✅ set the path after parsing
            std::cout << "[DEBUG] Parsed location path: '" << route.path << "'" << std::endl;
            server.routes.push_back(route);
        }

		else
		{
			std::cerr << "Unknown directive in server block: " << directive << std::endl;

			// Skip to next semicolon or closing brace
			while (current < tokens.size() && tokens[current].value != ";" && tokens[current].value != "}")
				current++;
			if (tokens[current].value == ";")
				current++;
		}
	}

	if (tokens[current].value != "}")
		throw std::runtime_error("Expected '}' to close server block");

	current++; // consume '}'
	return server;
}


RouteConfig ConfigParser::parseLocationBlock(const std::vector<Token>& tokens, size_t &current)
{
    RouteConfig ret;

    while (current < tokens.size() && tokens[current].value != "}")
    {
        std::string raw = tokens[current].value;
        if (raw.empty() || raw[0] == '#')
        {
            ++current;
            continue;
        }

        std::string directive = raw;
        ++current;
         if (directive == "methods" || directive == "method" || directive == "allowed_methods") {
            while (tokens[current].value != ";") {
                ret.allowed_methods.insert(tokens[current++].value);
                if (current >= tokens.size())
                    throw std::runtime_error("Unexpected end of allowed_methods");
            }
            current++;
        }
        else if (directive == "root") {
            ret.root = tokens[current++].value;
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after root");
        }
        else if (directive == "index") {
            ret.index = tokens[current++].value;
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after index");
        }
        else if (directive == "upload_path") {
            ret.upload_path = tokens[current++].value;
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after upload_path");
        }
        else if (directive == "autoindex") {
            std::string value = tokens[current++].value;
            ret.autoindex = (value == "on");
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after autoindex");
        }
        // new parse for cgi
        else if (directive == "cgi_extension") {
            if (current + 1 >= tokens.size())
                throw std::runtime_error("Expected: cgi_extension <ext> <interpreter>;");

            std::string ext = tokens[current++].value;        // ".py" or "py"
            std::string interpreter = tokens[current++].value; // path or "" (shebang)

            if (!ext.empty() && ext[0] != '.')
                ext = "." + ext;

            ret.cgi_extension[ext] = interpreter;

            if (current >= tokens.size() || tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after cgi_extension");
        }

        else if (directive == "cgi_path") {
            if (current >= tokens.size())
                throw std::runtime_error("Expected path after cgi_path");
            ret.cgi_path = tokens[current++].value;
            if (current >= tokens.size() || tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after cgi_path");
        }
        else if (directive == "cgi_timeout_ms") {
            if (current >= tokens.size())
                throw std::runtime_error("Expected numeric value after cgi_timeout_ms");
            std::string v = tokens[current++].value;
            std::istringstream is(v);
            size_t ms = 0;
            if (!(is >> ms))
                throw std::runtime_error("Invalid cgi_timeout_ms: " + v);
            ret.cgi_timeout_ms = ms;
            if (current >= tokens.size() || tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after cgi_timeout_ms");
        }
        // cgi_max_output_bytes 10485760;   (# bytes)
        else if (directive == "cgi_max_output_bytes") {
            if (current >= tokens.size())
                throw std::runtime_error("Expected numeric value after cgi_max_output_bytes");
            std::string v = tokens[current++].value;
            std::istringstream is(v);
            size_t cap = 0;
            if (!(is >> cap))
                throw std::runtime_error("Invalid cgi_max_output_bytes: " + v);
            ret.cgi_max_output_bytes = cap;
            if (current >= tokens.size() || tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after cgi_max_output_bytes");
        }

        // cgi_pass_env VAR1 VAR2 VAR3;
        else if (directive == "cgi_pass_env") {
            ret.cgi_pass_env.clear();

            // Collect tokens until ';'
            if (current >= tokens.size())
                throw std::runtime_error("Expected at least one var or ';' after cgi_pass_env");

            while (current < tokens.size() && tokens[current].value != ";") {
                ret.cgi_pass_env.push_back(tokens[current++].value);
            }
            if (current >= tokens.size() || tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after cgi_pass_env");
        }
        else if (directive == "max_body_size") {
            ret.max_body_size = std::atoi(tokens[current++].value.c_str());
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after max_body_size");
        }
        else if (directive == "redirect") {
            ret.redirect = tokens[current++].value;
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after redirect");
        }
        else {
            std::cerr << "Unknown directive in location: " << directive << std::endl;
            while (tokens[current].value != ";" && tokens[current].value != "}")
                current++;
            if (tokens[current].value == ";")
                current++;
        }
    }

    if (tokens[current].value != "}")
        throw std::runtime_error("Expected '}' to close location block");
    current++;
    if (ret.allowed_methods.empty())
    {
        ret.allowed_methods.insert("GET"); // default here, HEAD is added later via utils functions
    }

    std::cout << "[DEBUG] Parsed location index: '" << ret.index << "'" << std::endl;
    return ret;
}



//some printers for debug

void printRouteConfig(const RouteConfig &route) {
    std::cout << "  Location path: " << route.path << std::endl;
    
    std::cout << "    allowed_methods: ";
    for (std::set<std::string>::const_iterator it = route.allowed_methods.begin();
         it != route.allowed_methods.end(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    std::cout << "    root: " << route.root << std::endl;
    std::cout << "    index: " << route.index << std::endl;
    std::cout << "    upload_path: " << route.upload_path << std::endl;
    std::cout << "    autoindex: " << (route.autoindex ? "on" : "off") << std::endl;
    for (std::map<std::string, std::string>::const_iterator it = route.cgi_extension.begin(); it != route.cgi_extension.end(); it++)
        std::cout << "    cgi_extension: " << it->first << " & " << it->second << std::endl;
    std::cout << "    cgi_path: " << route.cgi_path << std::endl;
    std::cout << "    max_body_size: " << route.max_body_size << std::endl;
    std::cout << "    redirect: " << route.redirect << std::endl;
}

void printServerConfig(const ServerConfig& server) {
    std::cout << "=== Server Config ===" << std::endl;
    std::cout << "host: " << server.host << std::endl;
    std::cout << "port: " << server.port << std::endl;
    std::cout << "server_name: " << server.server_name << std::endl;
    std::cout << "root: " << server.root << std::endl;
    std::cout << "index: " << server.index << std::endl;
    std::cout << "client_max_body_size: " << server.client_max_body_size << std::endl;

    std::cout << "error_pages:" << std::endl;
    for (std::map<int, std::string>::const_iterator it = server.error_pages.begin();
         it != server.error_pages.end(); ++it) {
        std::cout << "  " << it->first << " => " << it->second << std::endl;
    }

    std::cout << "routes:" << std::endl;
    for (size_t i = 0; i < server.routes.size(); ++i) {
        printRouteConfig(server.routes[i]);
    }
}
