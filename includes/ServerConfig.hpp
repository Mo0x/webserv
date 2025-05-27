#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

/*ALMOST THE SAME AS CONFIG.HPP NEED TO DELETE ONE*/


#include <string>
#include <map>
#include <vector>

struct LocationConfig
{
	std::string path; // "/images"
	std::string root;                         // "/var/www/images"
    std::string index;                        // "index.html"
    std::vector<std::string> allowedMethods;  // {"GET", "POST"}
    bool autoindex;                           // true / false
    std::string redirect;                     // "301 http://example.com" (optional)
    std::string uploadPath;                   // "/tmp/uploads"
    std::string cgiPath;                      // "/usr/bin/php-cgi"
    std::string cgiExtension;                 // ".php"
};

class ServerConfig
{
	private:
	std::string					m_host; //"127.0.0.1"
	int							m_port; //8080
	std::string					m_servername; //optional
	std::map<int, std::string>	m_error_pages;
	std::vector<LocationConfig>	m_locations;
	size_t 						m_clientMaxBodySize;

	public:

	//Form canonical
	ServerConfig();
	ServerConfig(const ServerConfig &src);
	ServerConfig &operator=(const ServerConfig &src);
	~ServerConfig();

	//Get
	const std::string& getHost() const;
	int getPort() const;
	const std::string& getServerName() const;
	const std::map<int, std::string>& getErrorPages() const;
	size_t getClientMaxBodySize() const;
	const std::vector<LocationConfig>& getLocations() const;

	//Set
	void setHost(const std::string& host);
	void setPort(int port);
	void setServerName(const std::string& name);
	void addErrorPage(int code, const std::string& path);
	void setClientMaxBodySize(size_t size);
	void addLocation(const LocationConfig& loc);
};

#endif
