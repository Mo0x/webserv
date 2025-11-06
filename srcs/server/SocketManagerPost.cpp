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

//create a name, and avoid collision (using timestamp from epoch)
static std::string makeUploadFileName(const std::string &hint)
{
	static unsigned long s_counter = 0;
	++s_counter;

	// avoid slash from hint

	std::string base = hint;
	size_t pos = base.rfind('/');
	if (pos != std::string::npos)
		base = base.substr(pos +1);
	if (base.empty())
		base = "upload";
	
	//strip dangereous chars (basically we only take AZ_az-09.
	for (size_t i = 0; i < base.size(); ++i)
	{
		char &c = base[i];
		if (!(c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '_' || c == '-' || c == '.'
		)
		{
			c = '_';
		}
	}

	std::ostringstream oss;
	oss << base << (unsigned long)time(NULL) << "_" << s_counter;
	return oss.str();
}
