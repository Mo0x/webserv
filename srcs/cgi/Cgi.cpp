#include "SocketManager.hpp"
#include "Config.hpp"
#include <sys/time.h>

Cgi::Cgi() :
	pid(-1),
	stdin_w(-1),
	stdout_r(-1),
	stdin_closed(-1),
	headersParsed(false),
	cgiStatus(200),
	bytesInTotal(0),
	bytesOutTotal(0),
	tStartMs(0ULL)
{
	inBuf.clear();
	outBuf.clear();
	cgiHeaders.clear();
	scriptFsPath.clear();
	workingDir.clear();
}
static std::string getFileExtension(const std::string &path)
{
	// return extension with leading '.', or empty if none
	std::string::size_type slash = path.rfind('/');
	std::string::size_type dot   = path.rfind('.');
	if (dot == std::string::npos) return "";
	if (slash != std::string::npos && dot < slash) return "";
	return path.substr(dot); // includes '.'
}

static std::string joinPaths(const std::string &a, const std::string &b)
{
	if (a.empty()) return b;
	if (b.empty()) return a;
	if (a[a.size()-1] == '/' && b[0] == '/') return a + b.substr(1);
	if (a[a.size()-1] != '/' && b[0] != '/') return a + "/" + b;
	return a + b;
}

unsigned long long now_ms()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return static_cast<unsigned long long>(tv.tv_sec * 1000ULL + (tv.tv_usec / 1000ULL));
}

void Cgi::reset()
{
	pid            = -1;
	stdin_w        = -1;
	stdout_r       = -1;
	stdin_closed   = false;

	inBuf.clear();
	outBuf.clear();

	headersParsed  = false;
	cgiStatus      = 200;

	cgiHeaders.clear();

	bytesInTotal   = 0;
	bytesOutTotal  = 0;

	tStartMs       = 0ULL;
	scriptFsPath.clear();
	workingDir.clear();
}