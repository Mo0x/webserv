#include "Config.hpp"

ServerConfig::ServerConfig () :
	host("127.0.0.1"), port(8080), client_max_body_size(1000000)
{
	return ;
}

RouteConfig::RouteConfig() :
	autoindex(false),
	max_body_size(0),
	cgi_path(""),
	cgi_timeout_ms(5000),
	cgi_max_output_bytes(5 * 1024 * 1024)
{
	return ;
}
