
#include "file_utils.hpp"
#include <fstream>    
#include <sstream>   
#include <vector>    
#include <map>      
#include <string>    
#include <sys/stat.h> 

namespace file_utils
{

	bool exists(const std::string& path)
	{
		struct stat st;
		return (stat(path.c_str(), &st) == 0);
	}
	
	std::string readFile(const std::string& path)
	{
		std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
		if (!in) return "";
		std::ostringstream ss;
		ss << in.rdbuf();
		return ss.str();
	}
	
	// Simplest lexical normalization: collapse “..” and “.”
	std::string normalize(const std::string& raw)
	{
		std::vector<std::string> parts;
		std::istringstream iss(raw);
		std::string token;
		// split on '/'
		while (std::getline(iss, token, '/'))
		{
			if (token == "" || token == ".") continue;
			if (token == ".." && !parts.empty())
				parts.pop_back();
			else if (token != "..")
				parts.push_back(token);
		}
		std::string result = raw.size() && raw[0] == '/' ? "/" : "";
		for (size_t i = 0; i < parts.size(); ++i)
		{
			result += parts[i];
			if (i + 1 < parts.size()) result += "/";
		}
		return result;
	}
	
	std::string getMimeType(const std::string& path)
	{
		// Fill map once
		static std::map<std::string, std::string> m;
		if (m.empty())
		{
			m[".html"] = "text/html";
			m[".htm"]  = "text/html";
			m[".css"]  = "text/css";
			m[".js"]   = "application/javascript";
			m[".jpg"]  = "image/jpeg";
			m[".jpeg"] = "image/jpeg";
			m[".png"]  = "image/png";
			m[".gif"]  = "image/gif";
			m[""]      = "application/octet-stream";
		}
		size_t dot = path.rfind('.');
		std::string ext = (dot == std::string::npos ? "" : path.substr(dot));
		std::map<std::string, std::string>::iterator it = m.find(ext);
		return (it != m.end() ? it->second : "application/octet-stream");
	}
	
	} // namespace file_utils