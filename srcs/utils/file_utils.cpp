#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>

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

bool isPathSafe(const std::string& basePath, const std::string& fullPath)
{
	/* 
		use realpth to resolve things like "." ->current dir
		"./www" -> "/home/user/webserv/www"
	 */
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
}