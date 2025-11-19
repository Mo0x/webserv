#include "SocketManager.hpp"
#include "Config.hpp"
#include "request_reponse_struct.hpp"
#include "utils.hpp"
#include "file_utils.hpp"
#include "Chunked.hpp"
#include <sstream>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>
#include <map>
#include <cstdlib>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>

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

static std::string stripLocationPrefix(const std::string &url, const std::string &locPath)
{
	if (locPath.empty()) return url;
	if (url.size() < locPath.size()) return url;
	if (url.compare(0, locPath.size(), locPath) != 0) return url;
	std::string s = url.substr(locPath.size());
	if (!s.empty() && s[0] == '/') s.erase(0, 1);
	return s;
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


/*
	1) prefer upload_path when present
		build safe absolute path in upload dir
	2) Else: CGI hook (if your config indicates CGI for this route/path)
    If you already have a CGI runner, call it here with `body` as stdin.
    // Example sketch:
    // if (route && route->cgi_enabled) {
    //     runCgi(fd, req, server, *route, body); // must call finalizeAndQueue inside
    //     return;
    // }
*/

//TODO review this FUNCTIOn !!! Understand

void SocketManager::startCgiDispatch(int fd,
                                     ClientState &st,
                                     const ServerConfig &server,
                                     const RouteConfig &route)
{
	// Decide working directory: prefer cgi_path, else route.root, else server.root
	std::string workingDir = !route.cgi_path.empty() ? route.cgi_path
						: (!route.root.empty()   ? route.root : server.root);

	// Map URL to filesystem path under workingDir
	const std::string rel  = stripLocationPrefix(st.req.path, route.path);
	const std::string scriptFsPath = joinPaths(workingDir, rel);

	// Fill CGI state
	st.cgi.reset();                      // if you added reset(); else set fields like in your ctor
	st.cgi.workingDir    = workingDir;
	st.cgi.scriptFsPath  = scriptFsPath;
	st.cgi.inBuf.swap(st.bodyBuffer);    // move decoded request body to CGI stdin buffer
	st.cgi.tStartMs      = now_ms();
	st.cgi.stdin_w       = -1;
	st.cgi.stdout_r      = -1;
	st.cgi.stdin_closed  = st.cgi.inBuf.empty();
	st.cgi.headersParsed = false;
	st.cgi.cgiStatus     = 200;

	// Ensure multipart is bypassed for CGI routes
	st.isMultipart = false;
	st.multipartInit = false;
	st.mpState = ClientState::MP_START;
	st.multipartError = false;
	st.multipartStatusCode = 0;
	st.multipartStatusTitle.clear();
	st.multipartStatusBody.clear();

	// Flip outer phase — the event loop will later spawn/drive the CGI
	setPhase(fd, st, ClientState::CGI_RUNNING, "startCgiDispatch");

	// ---------- SPAWN + PIPE (O_NONBLOCK) + REGISTER -----------------
	int inPipe[2], outPipe[2];
	if (::pipe(inPipe)  < 0 || ::pipe(outPipe) < 0) {
		Response err = makeHtmlError(502, "Bad Gateway", "<h1>502 Bad Gateway</h1><p>pipe() failed.</p>");
		finalizeAndQueue(fd, st.req, err, /*body_expected=*/false, /*body_fully_consumed=*/true);
		return;
	}
	int fl;
	fl = fcntl(inPipe[1], F_GETFL, 0);  if (fl != -1) fcntl(inPipe[1], F_SETFL, fl | O_NONBLOCK);
	fl = fcntl(outPipe[0], F_GETFL, 0); if (fl != -1) fcntl(outPipe[0], F_SETFL, fl | O_NONBLOCK);
	pid_t pid = ::fork();
	if (pid < 0) {
		::close(inPipe[0]); ::close(inPipe[1]);
		::close(outPipe[0]); ::close(outPipe[1]);
		Response err = makeHtmlError(502, "Bad Gateway", "<h1>502 Bad Gateway</h1><p>fork() failed.</p>");
		finalizeAndQueue(fd, st.req, err, false, true);
		return;
	}

	if (pid == 0) {
		// ---- CHILD ----
		// stdio hookup
		::dup2(inPipe[0],  STDIN_FILENO);
		::dup2(outPipe[1], STDOUT_FILENO);
		// close inherited ends we don't need
		::close(inPipe[1]); ::close(outPipe[0]);

		// close any listening/client fds you keep in m_pollfds to avoid leaks in child
		// (Optional: iterate and close every fd except 0/1/2)
		for (size_t i = 0; i < m_pollfds.size(); ++i) {
			int cfd = m_pollfds[i].fd;
			if (cfd > 2) ::close(cfd);
		}

		// chdir to working dir
		if (!st.cgi.workingDir.empty()) ::chdir(st.cgi.workingDir.c_str());

        // ---- Build argv (use FS extension, not URL) ----
        std::vector<char*> argv;
        {
                std::string ext;
                size_t dot = st.cgi.scriptFsPath.rfind('.');
                if (dot != std::string::npos) ext = st.cgi.scriptFsPath.substr(dot);

                std::map<std::string,std::string>::const_iterator itInterp = route.cgi_extension.find(ext);
                if (itInterp != route.cgi_extension.end() && !itInterp->second.empty()) {
                        argv.push_back(const_cast<char*>(itInterp->second.c_str()));          // interpreter
                        argv.push_back(const_cast<char*>(st.cgi.scriptFsPath.c_str()));       // script
                } else {
                        argv.push_back(const_cast<char*>(st.cgi.scriptFsPath.c_str()));       // direct exec (shebang)
                }
                argv.push_back(NULL);
        }

        // ---- Build envp (RFC 3875 core) ----
        std::vector<std::string> env; env.reserve(64);

        // 1) URL parts and addresses
        std::string urlPath, query; splitPathAndQuery(st.req.path, urlPath, query);
        std::string remoteAddr, remotePort, serverAddr, serverPort;
        getSocketAddrs(fd, remoteAddr, remotePort, serverAddr, serverPort);

        // SERVER_NAME/PORT (prefer Host header if present)
        std::string hostHeader;
        { std::map<std::string,std::string>::const_iterator itH=st.req.headers.find("host");
          if (itH!=st.req.headers.end()) hostHeader=itH->second; }
        std::string serverName = server.server_name.empty() ? serverAddr : server.server_name;
        std::string serverPortStr = serverPort;
        if (!hostHeader.empty()) {
                std::string h = hostHeader;
                while(!h.empty()&&(h[0]==' '||h[0]=='\t')) h.erase(0,1);
                while(!h.empty()&&(h[h.size()-1]==' '||h[h.size()-1]=='\t')) h.erase(h.size()-1);
                size_t c=h.rfind(':');
                if (c!=std::string::npos) { serverName=h.substr(0,c); serverPortStr=h.substr(c+1); }
                else serverName=h;
        }

        // 2) Core vars
        env.push_back("GATEWAY_INTERFACE=CGI/1.1");
        env.push_back("REQUEST_METHOD="+st.req.method);
        env.push_back("SERVER_PROTOCOL="+st.req.http_version);
        env.push_back("SERVER_SOFTWARE=webserv/0.1");
        env.push_back("SERVER_NAME="+serverName);
        env.push_back("SERVER_PORT="+serverPortStr);

        // SCRIPT fields
        env.push_back("SCRIPT_FILENAME="+st.cgi.scriptFsPath);
        env.push_back("SCRIPT_NAME="+makeScriptName(route.path, urlPath)); // URL path as seen by client

        // REQUEST_URI & QUERY_STRING
        env.push_back("REQUEST_URI="+urlPath+(query.empty()?"":"?"+query));
        env.push_back("QUERY_STRING="+query);

        // Document root (optional)
        {
                std::string docroot = !route.root.empty() ? route.root : server.root;
                if (!docroot.empty()) env.push_back("DOCUMENT_ROOT="+docroot);
        }

        // Client info
        if (!remoteAddr.empty()) env.push_back("REMOTE_ADDR="+remoteAddr);
        if (!remotePort.empty()) env.push_back("REMOTE_PORT="+remotePort);

        // CONTENT_* (use actual stdin size we feed)
        if (!st.cgi.inBuf.empty()){
                std::ostringstream oss; oss<<st.cgi.inBuf.size();
                env.push_back("CONTENT_LENGTH="+oss.str());
        }
        { std::map<std::string,std::string>::const_iterator itCT=st.req.headers.find("content-type");
          if (itCT!=st.req.headers.end()) env.push_back("CONTENT_TYPE="+itCT->second); }

        // HTTP_* for all other headers
        for (std::map<std::string,std::string>::const_iterator it=st.req.headers.begin();
             it!=st.req.headers.end(); ++it)
        {
                const std::string &k = it->first;
                if (k=="content-type" || k=="content-length") continue;
                env.push_back("HTTP_"+httpKeyToCgiVar(k)+"="+it->second);
        }

        // Honor cgi_pass_env
        for (size_t i=0;i<route.cgi_pass_env.size();++i){
                const std::string &key = route.cgi_pass_env[i];
                const char* v = std::getenv(key.c_str());
                if (v && *v) env.push_back(key+"="+std::string(v));
        }

        // Convert to char*[]
        std::vector<char*> envp; envp.reserve(env.size()+1);
        for (size_t i=0;i<env.size();++i) envp.push_back(const_cast<char*>(env[i].c_str()));
        envp.push_back(NULL);

        // exec
        ::execve(argv[0], &argv[0], &envp[0]);
        _exit(127); // exec failed
	}

	// ---- PARENT ----
	::close(inPipe[0]);
	::close(outPipe[1]);

	st.cgi.pid      = pid;
	st.cgi.stdin_w  = inPipe[1];
	st.cgi.stdout_r = outPipe[0];

	// If empty body, close stdin immediately (CGI often waits for EOF)
	if (st.cgi.inBuf.empty()) {
		::close(st.cgi.stdin_w);
		st.cgi.stdin_w = -1;
		st.cgi.stdin_closed = true;
	} else {
		st.cgi.stdin_closed = false;
	}

	// Register pipe fds in poll()
	addPollFd(st.cgi.stdout_r, POLLIN);
	m_cgiStdoutToClient[st.cgi.stdout_r] = fd;

	if (st.cgi.stdin_w != -1) {
		addPollFd(st.cgi.stdin_w, POLLOUT);
		m_cgiStdinToClient[st.cgi.stdin_w] = fd;
	}

	std::cerr << "[fd " << fd << "] CGI spawned pid=" << pid
			<< " stdin_w=" << st.cgi.stdin_w
			<< " stdout_r=" << st.cgi.stdout_r << std::endl;
	// Optional debug
	std::cerr << "[fd " << fd << "] CGI dispatch : wd=" << st.cgi.workingDir.c_str() << "script=" << st.cgi.scriptFsPath.c_str() << std::endl;
}

void SocketManager::handlePostUploadOrCgi(int fd, 
									const Request &req,
									const ServerConfig &server,
									const RouteConfig *route,
									const std::string &body)
{
	    (void)server; // unused for now; kept for future CGI/env or policy checks 
	if (route && !route->upload_path.empty())
	{
		const std::string &dir = route->upload_path;
		//dir must exist
		if (!dirExists(dir))
		{
			Response res = makeHtmlError(409, "Conflict",  "<h1>409 Conflict</h1><p>Upload directory missing.</p>");
			finalizeAndQueue(fd, req, res, /*body_expected=*/false, /*body_fully_consumed=*/true);
			return;
		}
		//building a safe absolute path within upload dir
		std::string fname = makeUploadFileName(req.path);
		std::string full = joinPath(dir, fname);
		if (!isPathSafe(dir,full))
		{
			Response res = makeHtmlError(403, "Forbidden", "<h1>403 Forbidden</h1>");
			finalizeAndQueue(fd, req, res, false, true);
			return ;
		}
		// writefile
		std::ofstream ofs(full.c_str(), std::ios::out | std::ios:: binary | std::ios::trunc);
		if (!ofs)
		{
			Response res = makeHtmlError(500, "Internal Server Error",
										"<h1>500 Internal Server Error</h1>");
			finalizeAndQueue(fd, req, res, false, true);
			return;
		}
		if (!body.empty())
			ofs.write(&body[0], static_cast<std::streamsize>(body.size()));
		ofs.close();
		Response res;
		res.status_code = 201;
		res.status_message = "Created";
		res.headers["Content-Type"] = "text/plain; charset=utf-8";
		res.body = "Uploaded as: " + fname + "\n";
		res.headers["Content-Length"] = to_string(res.body.size());
		// Optionally expose Location relative to the request route
		// res.headers["Location"] = joinPath(req.path, fname) will see if needed;
		finalizeAndQueue(fd, req, res, false, true);
		return;
	}
	// 2) Else: CGI hook (if your config indicates CGI for this route/path)
	// If you already have a CGI runner, call it here with `body` as stdin.
	// Example sketch:
	// if (route && route->cgi_enabled) {
	//     runCgi(fd, req, server, *route, body); // must call finalizeAndQueue inside
	//     return;
	// }

	// 3) Nothing to do for POST on this route
	{
		Response res = makeHtmlError(404, "Not Found",
									"<h1>404 Not Found</h1><p>No upload or CGI here.</p>");
		finalizeAndQueue(fd, req, res, false, true);
	}
}

static std::string httpKeyToCgiVar(const std::string &k)
{
    std::string r; r.reserve(k.size());
    for (size_t i = 0; i < k.size(); ++i)
    {
        unsigned char c = (unsigned char)k[i];
        if (c == '-') r.push_back('_');
        else if (c >= 'a' && c <= 'z') r.push_back((char)(c - 'a' + 'A'));
        else r.push_back((char)c);
    }
    return r;
}


static void splitPathAndQuery(const std::string &raw, std::string &urlPath, std::string &query)
{
    size_t q = raw.find('?');
    if (q == std::string::npos) { urlPath = raw; query.clear(); }
    else { urlPath = raw.substr(0, q); query = raw.substr(q + 1); }
}

static void splitScriptAndPathInfo(const std::string &urlPath,
                                   const std::map<std::string,std::string> &cgiExtMap,
                                   std::string &scriptUrlPath,
                                   std::string &pathInfo)
{
    scriptUrlPath.clear();
    pathInfo.clear();

    // Walk from end: find last '/' to split components
    size_t lastSlash = urlPath.rfind('/');
    std::string lastComp = (lastSlash == std::string::npos) ? urlPath : urlPath.substr(lastSlash + 1);

    // Find a dot in last component
    size_t dot = lastComp.rfind('.');
    if (dot != std::string::npos)
    {
        std::string ext = lastComp.substr(dot); // includes '.'
        if (cgiExtMap.find(ext) != cgiExtMap.end())
        {
            // Script is up to end of this component
            scriptUrlPath = urlPath;
            pathInfo.clear(); // no extra after filename in this heuristic
            return;
        }
    }

    // Fallback: no recognized extension in last component.
    // We’ll assume the whole path is the script (no PATH_INFO).
    scriptUrlPath = urlPath;
    pathInfo.clear();
}

static void getSocketAddrs(int clientFd,
                           std::string &remoteAddr, std::string &remotePort,
                           std::string &serverAddr, std::string &serverPort)
{
    remoteAddr.clear(); remotePort.clear();
    serverAddr.clear(); serverPort.clear();

    sockaddr_storage peer; socklen_t plen = sizeof(peer);
    if (::getpeername(clientFd, (sockaddr*)&peer, &plen) == 0)
    {
        char h[NI_MAXHOST]; char s[NI_MAXSERV];
        if (::getnameinfo((sockaddr*)&peer, plen, h, sizeof(h), s, sizeof(s),
                          NI_NUMERICHOST | NI_NUMERICSERV) == 0)
        { remoteAddr = h; remotePort = s; }
    }

    sockaddr_storage self; socklen_t slen = sizeof(self);
    if (::getsockname(clientFd, (sockaddr*)&self, &slen) == 0)
    {
        char h[NI_MAXHOST]; char s[NI_MAXSERV];
        if (::getnameinfo((sockaddr*)&self, slen, h, sizeof(h), s, sizeof(s),
                          NI_NUMERICHOST | NI_NUMERICSERV) == 0)
        { serverAddr = h; serverPort = s; }
    }
}

static std::string makeScriptName(const std::string &routePrefix,
                                  const std::string &scriptUrlPath)
{
    // Ensure it starts with '/'
    if (scriptUrlPath.empty()) return "/";
    if (scriptUrlPath[0] != '/') return "/" + scriptUrlPath;
    return scriptUrlPath;
}

