#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>

bool fileExists(const std::string& path)
{
	struct stat buffer;
	return stat(path.c_str(), &buffer) == 0 && S_ISREG(buffer.st_mode);
}

std::string readFile(const std::string& path)
{
	std::ifstream file(path.c_str());
	std::ostringstream ss;
	ss << file.rdbuf();
	return ss.str();
}


bool isPathSafe(const std::string& base, const std::string& target)
{
	char resolvedBase[PATH_MAX];
	char resolvedTarget[PATH_MAX];

	if (!realpath(base.c_str(), resolvedBase))
		return false;
	if (!realpath(target.c_str(), resolvedTarget))
		return false;
	return std::string(resolvedTarget).find(std::string(resolvedBase)) == 0;
}