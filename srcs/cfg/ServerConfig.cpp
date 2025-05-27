#include "to_delete_ServerConfig.hpp"

/*to delete this file, keeping it for now in case we need to copy code from it*/

ServerConfig::ServerConfig()
	: m_host("0.0.0.0"), m_port(80), m_servername(""), m_error_pages(), m_locations(), m_clientMaxBodySize(1000000)
{
	return ;
}

ServerConfig::ServerConfig(const ServerConfig &src)
{
	*this = src;
}

ServerConfig &ServerConfig::operator=(const ServerConfig &src)
{
	if (this != &src) {
		m_host = src.m_host;
		m_port = src.m_port;
		m_servername = src.m_servername;
		m_error_pages = src.m_error_pages;
		m_locations = src.m_locations;
		m_clientMaxBodySize = src.m_clientMaxBodySize;
	}
	return *this;
}

ServerConfig::~ServerConfig()
{
	return ;
}

const std::string& ServerConfig::getHost() const
{
	return m_host;
}

int ServerConfig::getPort() const
{
	return m_port;
}

const std::string& ServerConfig::getServerName() const
{
	return m_servername;
}

const std::map<int, std::string>& ServerConfig::getErrorPages() const
{
	return m_error_pages;
}

size_t ServerConfig::getClientMaxBodySize() const
{
	return m_clientMaxBodySize;
}

const std::vector<LocationConfig>& ServerConfig::getLocations() const
{
	return m_locations;
}


// ===== Setters =====
void ServerConfig::setHost(const std::string &host)
{
	m_host = host;
}

void ServerConfig::setPort(int port)
{
	m_port = port;
}

void ServerConfig::setServerName(const std::string &name)
{
	m_servername = name;
}

void ServerConfig::addErrorPage(int code, const std::string &path)
{
	m_error_pages[code] = path;
}

void ServerConfig::setClientMaxBodySize(size_t size)
{
	m_clientMaxBodySize = size;
}

void ServerConfig::addLocation(const LocationConfig &loc)
{
	m_locations.push_back(loc);
}