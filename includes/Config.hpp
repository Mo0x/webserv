# ifndef CONFIG_HPP
# define CONFIG_HPP

# include <string>
# include <vector>
# include <set>

struct RouteConfig
{
    std::string path;
    std::set<std::string> allowed_methods;
    bool        autoindex = false;
    size_t      max_body_size = 0;
    std::string root;
    std::string index;
    std::string upload_path;
    std::string redirect;
    std::string cgi_extension;
    std::string cgi_path;
};

struct ServerConfig
{
    int         port = 80;
    std::string server_name;
    std::string root;
    std::string index;
    std::string error_page_404;
    std::vector<RouteConfig> routes;
};

struct Config
{
    std::vector<ServerConfig> servers;
};

#endif