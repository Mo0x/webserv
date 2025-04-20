#include <fstream>
#include <sstream>
#include <sys/stat.h>

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
