#include "Config.hpp"

ServerConfig::ServerConfig () :
	host("0.0.0.0"), port(80), client_max_body_size(1000000)
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
