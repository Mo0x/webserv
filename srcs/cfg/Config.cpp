#include "Config.hpp"

ServerConfig::ServerConfig () :
	host("0.0.0.0"), port(80), client_max_body_size(1000000)
{
	return ;
}