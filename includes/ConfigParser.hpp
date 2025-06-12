#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

//#include "ServerConfig.hpp"
#include "Config.hpp"
#include "ConfigLexer.hpp" 
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>

class ConfigParser
{
	private:
	std::string m_filePath;
	std::vector<ServerConfig> m_servers;

	void parse();                    // Parses entire file
	//void parseServer(std::istream&); // Parses a single `server` block

	//static std::string trim(const std::string& line);
	//static std::vector<std::string> split(const std::string& line);
	// Recursively parses a server block from tokens.
	ServerConfig parseServerBlock(const std::vector<Token>& tokens, size_t &current);
	RouteConfig parseLocationBlock(const std::vector <Token> tokens, size_t &current);

	public:
	ConfigParser();
	ConfigParser(const std::string &path);
	ConfigParser(const ConfigParser &src);
	ConfigParser &operator=(const ConfigParser &src);
	~ConfigParser();

	const std::vector<ServerConfig> &getServers() const;
};

	//some printer for debug
void printRouteConfig(const RouteConfig &route);
void printServerConfig(const ServerConfig &server);

#endif
