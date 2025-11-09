#include "utils.hpp"
#include <sstream>
#include <dirent.h>
#include "SocketManager.hpp"

std::string to_string(size_t val)
{
	std::ostringstream oss;
	oss << val;
	return oss.str();
}

bool isPathPrefix(const std::string& path, const std::string& prefix) {
	if (path.find(prefix) != 0)
		return false;

	if (path.length() == prefix.length())
		return true;

	// Make sure next char is '/' to avoid matching "/imageshack" with "/images"
	return path[prefix.length()] == '/';
}

/* version 2.0 of the routing logic because request with route like "/upload" werent 
matchin our /upload/ creating the helper matchOnePass for it*/

static const RouteConfig* matchOnePass(const ServerConfig &server, const std::string &reqPath)
{
	const RouteConfig *best_match = NULL;
	size_t best_length = 0;
	for (size_t i = 0; i < server.routes.size(); i++)
	{
		const RouteConfig &route = server.routes[i];
		const std::string &locPath = route.path;
		// prefix must be shared E.G does this request path start with locPath ? if not skip it
		if (reqPath.compare(0, locPath.size(), locPath) != 0)
			continue;
		/*here we check
		1. is the request path the same lenght as location path? I.e "/upload" with "/upload"
		2. if request path is stricly longer than locPath AND it ends with '/'?
		3. Does the location itself ends with '/'?
		*/
		bool exact = reqPath.size() == locPath.size();
		bool isNextSlash = (reqPath.size() > locPath.size() && reqPath[locPath.size()] == '/');
		bool locEndsWithSlash = (!locPath.empty() && locPath[locPath.size() - 1] == '/');
		if (exact || isNextSlash || locEndsWithSlash)
		{
			if (locPath.size() > best_length)
			{
				best_length = locPath.size();
				best_match = &route;
			}
		}
	}
	return best_match;
}

const RouteConfig* findMatchingLocation(const ServerConfig& server, const std::string& path)
{
	// first path we try exact as-is
	const RouteConfig *bestNormal = matchOnePass(server, path);
	const RouteConfig *bestWithSlash = NULL;
	if (!path.empty()  && path[path.size() - 1] != '/')
	{
		std::string alt = path;
		alt += '/';
		bestWithSlash = matchOnePass(server, alt);
	}
	if (bestNormal && !bestWithSlash)
	{
		return bestNormal;
	}
	if (!bestNormal && bestWithSlash)
		return bestWithSlash;
	if (bestNormal && bestWithSlash)
	{
		//we prefer longer and more specific one
		if (bestWithSlash->path.size() > bestNormal->path.size())
			return bestWithSlash;
		else
			return bestNormal;
	}
	return NULL;
}

/* THIS WORKS ON UNIX BUT NOT MAC BECAUSE IN THAT CASE d_type is alway UNKNOWN*/
std::string generateAutoIndexPage(const std::string &dirPath, const std::string &uriPath)
{
	std::ostringstream html;
	html << "<html><head><title> Index of " << uriPath << "</title></head><body>";
	html << "<h1>Index of " << uriPath << "</h1><ul>";
	DIR *dir = opendir(dirPath.c_str());
	if (!dir)
		return "<h1>403 Forbidden</h1>";
	
	struct dirent* entry;
	while((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;
		if (name == "." || name == "..") // <---- My choice to forbid ".." normally not a problem because path validation is clean but could lead to problem with weird dirs/config I.E directory traversing to a "admin directory"
			continue ;
		html << "<li> <a href=\"" << uriPath;
		if (uriPath[uriPath.size() - 1] != '/') 
			html << "/";
		html << name << "\">" << name << "</a> </li";
	}
	closedir(dir);
	html << "</ul></body></html>";
	return html.str();
}

std::string toUpperCopy(const std::string& str)
{
    std::string ret(str);
    for (size_t i = 0; i < ret.size(); ++i)
        ret[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(ret[i])));
    return ret;
}

// if GET is allowed, we must allow the HEAD method too : they share the same semantics; HEAD just omits the body
std::set<std::string> normalizeAllowedForAllowHeader(const std::set<std::string> &conf)
{
	std::set<std::string> out = conf;
	if (out.find("GET") != out.end())
		out.insert("HEAD");
	return out;
}

std::string joinAllowedMethods(const std::set<std::string> &methods)
{
	std::string out;
	for (std::set<std::string>::const_iterator it = methods.begin(); it != methods.end(); it++)
	{
		if (!out.empty())
			out += ", ";
		out += *it;
	}
	return out;
}

bool isMethodAllowedForRoute(const std::string &methodUpper, const std::set<std::string> &allowed)
{
	if (methodUpper == "HEAD")
		return (allowed.find("GET") != allowed.end());
	return (allowed.find(methodUpper) != allowed.end());
}

bool shouldCloseAfterThisResponse(int status, bool headers_complete, bool body_was_expected, bool body_fully_consumed, bool client_said_close)
{
	if (client_said_close)
		return true;
	if (!headers_complete)
		return true;
	if (status == 400 || status == 408)
		return true;
	if (status == 413 && body_was_expected && !body_fully_consumed)
		return true;
	if (status == 413 || status == 431)
		return true ;
	return false;
}

std::string trimCopy(const std::string &s)
{
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
    return s.substr(b, e - b);
}

std::string getMimeTypeFromPath(const std::string& path) 
{
	// find last dot (.) that comes after the last slash (/)
	size_t slash = path.find_last_of("/\\");
	size_t dot   = path.find_last_of('.');
	if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
		return "application/octet-stream";

	std::string ext = path.substr(dot + 1);
	// lowercase the extension safely
	for (size_t i = 0; i < ext.size(); ++i)
		ext[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[i])));

	if (ext == "html" || ext == "htm")  return "text/html; charset=utf-8";
	if (ext == "css")                   return "text/css";
	if (ext == "js")                    return "application/javascript";
	if (ext == "json")                  return "application/json";
	if (ext == "txt" || ext == "log")   return "text/plain; charset=utf-8";
	if (ext == "svg")                   return "image/svg+xml";
	if (ext == "png")                   return "image/png";
	if (ext == "jpg" || ext == "jpeg")  return "image/jpeg";
	if (ext == "gif")                   return "image/gif";
	if (ext == "webp")                  return "image/webp";
	if (ext == "ico")                   return "image/x-icon";
	if (ext == "pdf")                   return "application/pdf";
	// fallback
	return "application/octet-stream";
}

// Count how many times each header name appears (case-insensitive)
// we check each line, lowecase it and had to a map "value(hearder) key(count of header)"
// so if count > 1 we now there is a problem 

void countHeaderNames(const std::string &rawHeaders, std::map<std::string, size_t> &outCounts)
{
    outCounts.clear();
    size_t pos = 0;
    while (pos < rawHeaders.size())
    {
        size_t nl = rawHeaders.find("\r\n", pos);
        if (nl == std::string::npos) nl = rawHeaders.size();
        if (nl == pos) break; // empty line (defensive)

        const std::string line = rawHeaders.substr(pos, nl - pos);

        // only header lines have ':'
        size_t colon = line.find(':');
        if (colon != std::string::npos)
        {
            std::string name = line.substr(0, colon);

            // trim trailing spaces/tabs in header-name
            while (!name.empty() && (name[name.size() - 1] == ' ' || name[name.size() - 1] == '\t'))
                name.erase(name.size() - 1);

            // lowercase ASCII
            for (size_t i = 0; i < name.size(); ++i)
            {
                unsigned char c = static_cast<unsigned char>(name[i]);
                if (c >= 'A' && c <= 'Z')
                    name[i] = static_cast<char>(c - 'A' + 'a');
            }
            ++outCounts[name];
        }

        pos = (nl == rawHeaders.size()) ? nl : nl + 2; 
    }
}

std::string toLowerCopy(const std::string &str)
{
	std::string t;
	t.reserve(str.size());
	for (size_t i = 0; i < str.size(); ++i)
		t.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(str[i]))));
	return t;
}

void normalizeHeaderKeys(std::map<std::string, std::string> &hdrs)
{
	if (hdrs.empty())
		return;
	std::map<std::string, std::string> lower;
	for (std::map<std::string, std::string>::const_iterator it = hdrs.begin(); it != hdrs.end(); it++)
		lower[toLowerCopy(it->first)] = it->second;
	hdrs.swap(lower);
}

//lil helper for big boi std_to_hex

static int hex_value(char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	else if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	else if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	return -1;
}
//basically atoi_base but return a size_t and take a string no negative possible as its size_t

bool std_to_hex(const std::string &hex_part, size_t &ret)
{
	ret = 0;
	if (hex_part.empty())
		return false;
	size_t i = 0;
	while (i < hex_part.size())
	{
		int v = hex_value(hex_part[i]);
		if (v < 0)
			return false;
		//in 64 bit : casting -1 to size_t == 0xFFFFFFFF(etc...) with >> 4 we divide by 16 
		//so we get the largest number that multiplied by 16 still fit size_t so 0x0FFFFFFFF(etc...)
		// so if ret is biggger than this we know we are about to overflow
		if (ret > ((size_t)(-1) >> 4))
			return false;
		ret = (ret << 4) + (size_t)v;
		i++;
	}
	return true;
}