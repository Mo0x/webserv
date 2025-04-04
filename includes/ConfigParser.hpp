#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "ServerConfig.hpp"
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
	void parseServer(std::istream&); // Parses a single `server` block

	static std::string trim(const std::string& line);
	static std::vector<std::string> split(const std::string& line);

	public:
	ConsfigParser();
	ConfigParser(const ConfigParser &src);
	ConfigParser &operator=(const ConfigParser &src);
	~ConfigParser();

	ConfigParser(const std::string &path);
	const std::vector<ServerConfig> &getServers() const;
};

#endif