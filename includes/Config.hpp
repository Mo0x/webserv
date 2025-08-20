# ifndef CONFIG_HPP
# define CONFIG_HPP

# include <string>
# include <vector>
# include <set>
# include <map>

struct RouteConfig
{
    std::string path;
    std::set<std::string> allowed_methods;
    bool        autoindex;
    size_t      max_body_size;
    std::string root;
    std::string index;
    std::string upload_path;
    std::string redirect;
    std::string cgi_extension;
    std::string cgi_path;
    
    RouteConfig();
};

struct ServerConfig
{
    std::string host;
    int         port;
    std::string server_name;
    std::string root;
    std::string index;
    std::map<int, std::string> error_pages;
    std::vector<RouteConfig> routes;
    size_t client_max_body_size;

    ServerConfig();
};

struct Config
{
    std::vector<ServerConfig> servers;
};

#endif