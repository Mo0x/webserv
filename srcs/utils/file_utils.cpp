#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <vector>

bool fileExists(const std::string& path)
{
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
}

std::string readFile(const std::string& path)
{
	std::ifstream file(path.c_str());
	std::ostringstream ss;
	ss << file.rdbuf();
	return ss.str();
}

bool isPathTraversalSafe(const std::string &path)
{
	return path.find("..") == std::string::npos;
}

// Normalize things like:
//   "/var/www/../www/site/./index.html" → "/var/www/site/index.html"
//   "www/./sub/../file"                → "www/file"
static std::string canonicalizePath(const std::string &path)
{
	bool absolute = !path.empty() && path[0] == '/';

	std::vector<std::string> parts;
	std::string current;

	for (size_t i = 0; i <= path.size(); ++i)
	{
		char c = (i < path.size()) ? path[i] : '/';
		if (c == '/')
		{
			if (!current.empty())
			{
				if (current == ".")
				{
					// ignore
				}
				else if (current == "..")
				{
					if (!parts.empty())
						parts.pop_back();
				}
				else
				{
					parts.push_back(current);
				}
				current.clear();
			}
		}
		else
		{
			current += c;
		}
	}

	std::string result;
	if (absolute)
		result = "/";

	for (size_t i = 0; i < parts.size(); ++i)
	{
		if (i > 0 || absolute)
			result += "/";
		result += parts[i];
	}

	if (result.empty())
		return absolute ? "/" : ".";

	return result;
}

bool isPathSafe(const std::string& basePath, const std::string& fullPath)
{
	// Canonical base and full paths (collapse . and ..)
	std::string base = canonicalizePath(basePath);
	std::string full = canonicalizePath(fullPath);

	if (base.empty() || full.empty())
		return false;

	// If one is absolute and the other is relative, treat as not safe.
	// (With realpath, both would be absolute and comparable.)
	bool baseAbs = !base.empty() && base[0] == '/';
	bool fullAbs = !full.empty() && full[0] == '/';
	if (baseAbs != fullAbs)
		return false;

	// Exact match (not really used in practice, but it's inside the base).
	if (full == base)
		return true;

	// Full path must start with base + '/' to avoid prefix trap.
	if (full.size() > base.size()
		&& full.compare(0, base.size(), base) == 0
		&& full[base.size()] == '/')
	{
		return true;
	}

	return false;
}


/* bool isPathSafe(const std::string& basePath, const std::string& fullPath)
{
		//use realpth to resolve things like "." ->current dir
		//"./www" -> "/home/user/webserv/www"
	char realBase[PATH_MAX];
	if (!realpath(basePath.c_str(), realBase))
		return false;

	// Canonical base path
	std::string base(realBase);

	// Canonical requested path's *parent directory*
	std::string dirPart = fullPath.substr(0, fullPath.find_last_of('/'));
	char realDir[PATH_MAX];
	if (!realpath(dirPart.c_str(), realDir))
		return false;

	std::string realParent(realDir);
	return (realParent.find(base) == 0);
} */

bool dirExists(const std::string &path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}