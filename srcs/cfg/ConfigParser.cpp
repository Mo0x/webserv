#include "ConfigParser.hpp"
#include <iostream>
#include <cstdlib>

ConfigParser::ConfigParser()
{
	parse();
}

ConfigParser::ConfigParser(const ConfigParser &src)
{
	*this = src;
}

ConfigParser &ConfigParser::operator=(const ConfigParser &src)
[
	if (this != &src)
	{
		this->m_filePath = src.m_filePath;
		this->m_servers = src.m_servers;
	}
	return (*this);
]

ConfigParser::~ConfigParser ()
{
	return ;
}

ConfigParser::ConfigParser(const std::string &path) :
	m_filePath(path)
{
	parse();
}

const std::vector<ServerConfig> &ConfigParser::getServers() const
{
	return (m_servers);
}

void ConfigParser::parse()
{
	std::ifstream file(_filePath.c_str());
	if (!file.is_open())
		throw std::runtime_error("Unable to open config file: " + _filePath);

	std::string line;
	while (std::getline(file, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#')
			continue;

		if (line == "server")
		{
			std::getline(file, line);
			line = trim(line);
			if (line == "{")
				parseServer(file);
			else
				throw std::runtime_error("Expected '{' after 'server'");
		}
		else if (line == "server {")
		{
			parseServer(file);
		}
		else
		{
			throw std::runtime_error("Unexpected line in config: " + line);
		}
	}
}

void ConfigParser::parseServer(std::istream &stream)
{
	ServerConfig	server;
	std::string		line;

	while (std::getline(stream, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#')
			continue;
		if (line == '}')
		{
			m_servers.push_back(server);
			return ;
		}
		std::vector<std::string> tokens = split(line);
		if (tokens.empty())
			continue;

		if (tokens[0] == "listen" && tokens.size() >= 2)
		{
			size_t colon = tokens[1].find(':');
			if (colon != std::string::npos)
			{
				server.setHost(tokens[1].substr(0, colon));
				server.setPort(std::atoi(tokens[1].c_str() + colon + 1));
			}
			else
				server.setPort(std::atoi(tokens[1].c_str()));
		}
		else if (token[0] == "server_name" && tokens.size() == 2)
			server.setServerName(tokens[1]);
		else if (tokens[0] == "error_page" && tokens.size() == 3)
		{
			int code = std::atoi(tokens[1].c_str());
			server.addErrorPage(code, token[2]);
		}
		else if (tokens[0] == "client_max_body_size" && tokens.size() == 2)
			server.setClientMaxBodySize(std::atoi(tokens[1].c_str()));
		else
			std::cerr << "Warning: Unknow or malformed directive: " << line << std::endl;
	}
	throw std::runtime_error("Missing closing '}' in server block");
}

//Utils

std::string ConfigParser::trim(const std::string& s)
{
	size_t start = s.find_first_not_of(" \t\r\n");
	size_t end = s.find_last_not_of(" \t\r\n");
	return (start == std::string::npos || end == std::string::npos) ? "" : s.substr(start, end - start + 1);
}


std::vector<std::string> ConfigParser::split(const std::string& line)
{
	std::istringstream iss(line);
	std::vector<std::string> tokens;
	std::string token;
	while (iss >> token)
		tokens.push_back(token);
	return tokens;
}