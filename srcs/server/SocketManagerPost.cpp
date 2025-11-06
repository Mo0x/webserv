#include "SocketManager.hpp"
#include "Config.hpp"
#include "request_reponse_struct.hpp"
#include "Chunked.hpp"
#include <iostream>

static std::string joinPath(const std::string &a, const std::string &b)
{
	if (a.empty())
		return b;
	if (b.empty())
		return a;

	if (a[a.size() - 1] == '/' )
	{
		if ( b[0] == '/')
			return a + b.substr(1);
		return a + b;
	}
	else
	{
		if (b[0] == '/')
			return a + b;
		return a + "/" + b;
	}
}
