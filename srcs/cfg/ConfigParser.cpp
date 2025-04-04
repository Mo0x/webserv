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