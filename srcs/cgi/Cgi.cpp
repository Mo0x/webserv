#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Config.hpp"
#include "SocketManager.hpp"

static const size_t CGI_HIGH_WATER = 1 << 20;  // 1 MiB
static const size_t CGI_LOW_WATER  = 1 << 19;  // 512 KiB

Cgi::Cgi() :
        pid(-1),
        stdin_w(-1),
        stdout_r(-1),
        stdin_closed(-1),
        stdoutPaused(false),
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


void Cgi::reset()
{
        pid            = -1;
        stdin_w        = -1;
        stdout_r       = -1;
        stdin_closed   = false;
        stdoutPaused   = false;

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

void SocketManager::addPollFd(int fd, short events)
{
	struct pollfd p; p.fd = fd; p.events = events; p.revents = 0;
	m_pollfds.push_back(p);
}
void SocketManager::modPollEvents(int fd, short setMask, short clearMask)
{
	for (size_t i = 0; i < m_pollfds.size(); ++i)
		if (m_pollfds[i].fd == fd) { m_pollfds[i].events |= setMask; m_pollfds[i].events &= ~clearMask; break; }
}

void SocketManager::delPollFd(int fd)
{
        for (size_t i = 0; i < m_pollfds.size(); ++i)
        {
                if (m_pollfds[i].fd == fd)
                {
                        m_pollfds.erase(m_pollfds.begin() + i);
                        break;
                }
        }
}

void SocketManager::pauseCgiStdoutIfNeeded(int clientFd, ClientState &st)
{
        (void)clientFd;
        if (st.cgi.stdout_r == -1 || st.cgi.stdoutPaused)
                return;
        if (st.writeBuffer.size() < CGI_HIGH_WATER)
                return;
        delPollFd(st.cgi.stdout_r);
        st.cgi.stdoutPaused = true;
}

void SocketManager::maybeResumeCgiStdout(int clientFd, ClientState &st)
{
        (void)clientFd;
        if (st.cgi.stdout_r == -1 || !st.cgi.stdoutPaused)
                return;
        if (st.writeBuffer.size() > CGI_LOW_WATER)
                return;
        addPollFd(st.cgi.stdout_r, POLLIN);
        st.cgi.stdoutPaused = false;
}

bool SocketManager::isCgiStdout(int fd) const
{
	return m_cgiStdoutToClient.find(fd) != m_cgiStdoutToClient.end();
}

bool SocketManager::isCgiStdin(int fd) const
{
	return m_cgiStdinToClient.find(fd) != m_cgiStdinToClient.end();
}

void SocketManager::handleCgiWritable(int pipefd)
{
    std::map<int,int>::iterator it = m_cgiStdinToClient.find(pipefd);
    if (it == m_cgiStdinToClient.end())
        return;

    int clientFd = it->second;
    ClientState &st = m_clients[clientFd];

    if (st.cgi.stdin_w != pipefd || st.cgi.stdin_closed)
    {
        delPollFd(pipefd);
        m_cgiStdinToClient.erase(it);
        return;
    }

    bool shouldClose = st.cgi.inBuf.empty();
    while (!st.cgi.inBuf.empty())
    {
        ssize_t n = ::write(pipefd, &st.cgi.inBuf[0], st.cgi.inBuf.size());
        if (n > 0)
        {
            st.cgi.inBuf.erase(0, static_cast<size_t>(n));
            st.cgi.bytesInTotal += static_cast<size_t>(n);
            shouldClose = st.cgi.inBuf.empty();
            continue;
        }

        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return;

            // Treat EPIPE (child closed stdin) the same as being done.
            shouldClose = true;
            break;
        }

        // n == 0
        shouldClose = true;
        break;
    }

    if (shouldClose)
    {
        ::close(pipefd);
        st.cgi.stdin_w = -1;
        st.cgi.stdin_closed = true;
        delPollFd(pipefd);
        m_cgiStdinToClient.erase(it);
    }
}

void SocketManager::handleCgiReadable(int pipefd)
{
    std::map<int,int>::iterator it = m_cgiStdoutToClient.find(pipefd);
    if (it == m_cgiStdoutToClient.end()) return;
    int clientFd = it->second;

    ClientState &st = m_clients[clientFd];
    char buf[4096];

    bool gotData = false;

    for (;;)
    {
        ssize_t n = ::read(pipefd, buf, sizeof(buf));
        if (n > 0)
        {
            st.cgi.outBuf.append(buf, static_cast<size_t>(n));
            gotData = true;                    // let drainCgiOutput handle counters
            continue;                          // try to coalesce more bytes this tick
        }
        else if (n == 0)
        {
            // EOF on CGI stdout
            ::close(pipefd);
            st.cgi.stdout_r = -1;
            delPollFd(pipefd);
            m_cgiStdoutToClient.erase(it);

            // Final drain to parse headers (if not yet) and finish body/close logic
            drainCgiOutput(clientFd);
            break;
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                break;                          // nothing more right now

            // hard error -> close pipe, stop polling
            ::close(pipefd);
            st.cgi.stdout_r = -1;
            delPollFd(pipefd);
            m_cgiStdoutToClient.erase(it);

            // Try to complete with whatever we buffered
            drainCgiOutput(clientFd);
            break;
        }
    }

    if (gotData)
        drainCgiOutput(clientFd);               // parse headers / push body / enforce caps
}


void SocketManager::handleCgiPipeError(int pipefd)
{
	if (isCgiStdout(pipefd)) 
    {
		int clientFd = m_cgiStdoutToClient[pipefd];
		delPollFd(pipefd); ::close(pipefd);
		m_cgiStdoutToClient.erase(pipefd);
		m_clients[clientFd].cgi.stdout_r = -1;
	}
	if (isCgiStdin(pipefd)) 
    {
		int clientFd = m_cgiStdinToClient[pipefd];
		delPollFd(pipefd); ::close(pipefd);
		m_cgiStdinToClient.erase(pipefd);
		m_clients[clientFd].cgi.stdin_w = -1;
		m_clients[clientFd].cgi.stdin_closed = true;
	}
}

void SocketManager::killCgiProcess(ClientState &st, int sig)
{
    if (st.cgi.pid > 0)
        kill(st.cgi.pid, sig);
}

void SocketManager::reapCgiIfDone(ClientState &st)
{
    if (st.cgi.pid <= 0)
        return;

    int status = 0;
    pid_t r = ::waitpid(st.cgi.pid, &status, WNOHANG);
    if (r > 0)
    {
        st.cgi.pid = -1;
        if (WIFEXITED(status))
            st.cgi.cgiStatus = WEXITSTATUS(status);
        else
            st.cgi.cgiStatus = 502;
    }
}

static void inject_header_lines_before_body(std::string &respBuf,
                                            const std::vector<std::string> &lines)
{
    if (lines.empty()) return;
    size_t p = respBuf.find("\r\n\r\n");
    if (p == std::string::npos) {
        p = respBuf.find("\n\n");
    }
    if (p == std::string::npos) {
        // Fallback: prepend (shouldn't happen with our finalize path)
        std::string block;
        for (size_t i = 0; i < lines.size(); ++i) {
            block += lines[i];
            if (block.size() < 2 || block.substr(block.size()-2) != "\r\n")
                block += "\r\n";
        }
        respBuf = block + respBuf;
        return;
    }
    std::string block;
    for (size_t i = 0; i < lines.size(); ++i) {
        block += lines[i];
        if (block.size() < 2 || block.substr(block.size()-2) != "\r\n")
            block += "\r\n";
    }
    respBuf.insert(p, block);
}

static bool isHopByHop(const std::string &k) {
    return k == "connection" || k == "transfer-encoding" ||
           k == "keep-alive" || k == "upgrade" ||
           (k.size() >= 6 && k.compare(0, 6, "proxy-") == 0);
}

static void applyCgiHeadersToResponse(const std::map<std::string,std::string> &h,
                                      Response &res,
                                      /*out*/ bool &sawLocation,
                                      /*out*/ int &cgiStatus)
{
    sawLocation = false;
    cgiStatus = 200;

    std::map<std::string,std::string>::const_iterator itStatus = h.find("status");
    if (itStatus != h.end()) {
        const std::string &v = itStatus->second;
        if (v.size() >= 3 &&
            std::isdigit(static_cast<unsigned char>(v[0])) &&
            std::isdigit(static_cast<unsigned char>(v[1])) &&
            std::isdigit(static_cast<unsigned char>(v[2])))
        {
            cgiStatus = (v[0]-'0')*100 + (v[1]-'0')*10 + (v[2]-'0');
            size_t sp = v.find(' ');
            res.status_code = cgiStatus;
            res.status_message = (sp != std::string::npos) ? v.substr(sp + 1) : "";
        }
    }

    for (std::map<std::string,std::string>::const_iterator it = h.begin();
         it != h.end(); ++it)
    {
        const std::string &k = it->first;
        const std::string &v = it->second;
        if (k == "status" || isHopByHop(k))
            continue;

        if (k == "location") {
            sawLocation = true;
            res.headers["Location"] = v;
            continue;
        }
        res.headers[k] = v; // retain lowercase unless caller normalizes
    }

    if (sawLocation && itStatus == h.end()) {
        res.status_code = 302;
        res.status_message = "Found";
        cgiStatus = 302;
    }

    if (res.status_code == 0) {
        res.status_code = cgiStatus;
        if (res.status_code == 0)
            res.status_code = 200;
    }
    if (res.status_message.empty()) {
        if (res.status_code == 200)
            res.status_message = "OK";
    }
}

bool SocketManager::parseCgiHeaders(ClientState &st, int clientFd, const RouteConfig &route)
{
    const size_t CGI_HEADER_CAP = 32 * 1024;
    (void)route;

    // Enforce a cap while waiting for header terminator
    if (st.cgi.outBuf.size() > CGI_HEADER_CAP &&
        st.cgi.outBuf.find("\r\n\r\n") == std::string::npos &&
        st.cgi.outBuf.find("\n\n") == std::string::npos)
    {
        // No header terminator within cap — treat as bad gateway
        killCgiProcess(st, SIGKILL);
        Response err = makeHtmlError(502, "Bad Gateway",
                                     "<h1>502 Bad Gateway</h1><p>CGI produced no headers.</p>");
        finalizeAndQueue(clientFd, st.req, err, false, true);
        // close pipes
        if (st.cgi.stdin_w  != -1) { ::close(st.cgi.stdin_w);  st.cgi.stdin_w  = -1; }
        if (st.cgi.stdout_r != -1) { ::close(st.cgi.stdout_r); st.cgi.stdout_r = -1; }
        reapCgiIfDone(st);
        return false;
    }

    // Find header end
    size_t pos = st.cgi.outBuf.find("\r\n\r\n");
    size_t delim = (pos != std::string::npos) ? pos + 4 : std::string::npos;
    if (delim == std::string::npos) {
        pos = st.cgi.outBuf.find("\n\n");
        if (pos == std::string::npos)
            return false; // need more data
        delim = pos + 2;
    }

    // Parse lines
    std::map<std::string, std::string> merged; // simple merge for non-duplicate headers
    std::vector<std::string> setCookieLines;   // keep raw duplicate Set-Cookie
    size_t start = 0;
    while (start < delim) {
        // find EOL
        size_t eol = st.cgi.outBuf.find("\r\n", start);
        size_t adv = 2;
        if (eol == std::string::npos || eol >= delim) {
            eol = st.cgi.outBuf.find('\n', start);
            if (eol == std::string::npos || eol >= delim) break;
            adv = 1;
        }
        if (eol == start) { start += adv; break; } // blank line
        std::string line = st.cgi.outBuf.substr(start, eol - start);
        start = eol + adv;

        // split at ':'
        size_t colon = line.find(':');
        if (colon == std::string::npos) {
            // ignore invalid header line (tolerant)
            continue;
        }
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        // trim OWS at start of value
        while (!val.empty() && (val[0] == ' ' || val[0] == '\t')) val.erase(0,1);
        key = toLowerCopy(key);

        if (key == "set-cookie") {
            setCookieLines.push_back("Set-Cookie: " + val);
            continue;
        }
        // merge others (comma-join)
        std::map<std::string,std::string>::iterator it = merged.find(key);
        if (it == merged.end()) merged[key] = val;
        else                    it->second += std::string(",") + val;
    }

    // Drop header block from outBuf
    st.cgi.outBuf.erase(0, delim);

    Response res;
    res.status_code = 0;
    res.status_message.clear();
    res.body.clear();
    res.close_connection = false;

    bool sawLocation = false;
    int cgiStatus = 200;
    applyCgiHeadersToResponse(merged, res, sawLocation, cgiStatus);
    st.cgi.cgiStatus = cgiStatus;

    // Queue headers. IMPORTANT: pass (body_expected=false, body_fully_consumed=true)
    // so we DON'T force-close right away; we'll stream the body manually.
    finalizeAndQueue(clientFd, st.req, res, /*body_expected=*/false, /*body_fully_consumed=*/true);

    // Inject duplicate Set-Cookie lines (Response::headers can't hold dupes)
    if (!setCookieLines.empty()) {
        inject_header_lines_before_body(st.writeBuffer, setCookieLines);
        setPollToWrite(clientFd);
        tryFlushWrite(clientFd, st);
    }

    st.cgi.headersParsed = true;
    // Cache normalized headers for later checks if you want:
    st.cgi.cgiHeaders.swap(merged);
    return true;
}


void SocketManager::drainCgiOutput(int clientFd)
{
    std::map<int, ClientState>::iterator itc = m_clients.find(clientFd);
    if (itc == m_clients.end())
        return;

    ClientState &st = itc->second;

    if (st.phase != ClientState::CGI_RUNNING && !clientHasPendingWrite(st))
        return;

    const ServerConfig &srv = m_serversConfig[m_clientToServerIndex[clientFd]];
    const RouteConfig  *matchedRoute  = findMatchingLocation(srv, st.req.path);
    RouteConfig fallbackRoute;
    const RouteConfig &route = matchedRoute ? *matchedRoute : fallbackRoute;
    const size_t timeout_ms = route.cgi_timeout_ms;
    const size_t max_bytes  = route.cgi_max_output_bytes;

    const unsigned long long elapsed = now_ms() - st.cgi.tStartMs;
    if (timeout_ms > 0 && elapsed > static_cast<unsigned long long>(timeout_ms))
    {
        killCgiProcess(st, SIGKILL);

        if (st.cgi.stdin_w != -1)
        {
            delPollFd(st.cgi.stdin_w);
            ::close(st.cgi.stdin_w);
            m_cgiStdinToClient.erase(st.cgi.stdin_w);
            st.cgi.stdin_w = -1;
            st.cgi.stdin_closed = true;
        }
        if (st.cgi.stdout_r != -1)
        {
            delPollFd(st.cgi.stdout_r);
            ::close(st.cgi.stdout_r);
            m_cgiStdoutToClient.erase(st.cgi.stdout_r);
            st.cgi.stdout_r = -1;
        }

        Response err = makeHtmlError(504, "Gateway Timeout", "<h1>504 Gateway Timeout</h1>");
        finalizeAndQueue(clientFd, st.req, err, false, true);
        reapCgiIfDone(st);
        return;
    }

    if (!st.cgi.headersParsed && !st.cgi.outBuf.empty())
    {
        if (parseCgiHeaders(st, clientFd, route))
        {
            setPollToWrite(clientFd);
            tryFlushWrite(clientFd, st);
        }
    }

    if (!st.cgi.headersParsed && st.cgi.stdout_r == -1)
    {
        killCgiProcess(st, SIGKILL);

        if (st.cgi.stdin_w != -1)
        {
            delPollFd(st.cgi.stdin_w);
            ::close(st.cgi.stdin_w);
            m_cgiStdinToClient.erase(st.cgi.stdin_w);
            st.cgi.stdin_w = -1;
            st.cgi.stdin_closed = true;
        }
        Response err = makeHtmlError(502, "Bad Gateway", "<h1>502 Bad Gateway</h1><p>CGI exited early.</p>");
        finalizeAndQueue(clientFd, st.req, err, false, true);
        reapCgiIfDone(st);
        return;
    }

    if (st.cgi.headersParsed && !st.cgi.outBuf.empty())
    {
        if (max_bytes > 0 &&
            st.cgi.bytesOutTotal + st.cgi.outBuf.size() > max_bytes)
        {
            killCgiProcess(st, SIGKILL);

            if (st.cgi.stdin_w != -1)
            {
                delPollFd(st.cgi.stdin_w);
                ::close(st.cgi.stdin_w);
                m_cgiStdinToClient.erase(st.cgi.stdin_w);
                st.cgi.stdin_w = -1;
                st.cgi.stdin_closed = true;
            }
            if (st.cgi.stdout_r != -1)
            {
                delPollFd(st.cgi.stdout_r);
                ::close(st.cgi.stdout_r);
                m_cgiStdoutToClient.erase(st.cgi.stdout_r);
                st.cgi.stdout_r = -1;
            }

            Response err = makeHtmlError(502, "Bad Gateway", "<h1>502 Bad Gateway</h1><p>CGI output too large.</p>");
            finalizeAndQueue(clientFd, st.req, err, false, true);
            reapCgiIfDone(st);
            return;
        }

        if (st.req.method == "HEAD")
        {
            st.cgi.bytesOutTotal += st.cgi.outBuf.size();
            st.cgi.outBuf.clear();
        }
        else
        {
            st.writeBuffer.append(st.cgi.outBuf);
            pauseCgiStdoutIfNeeded(clientFd, st);
            st.cgi.bytesOutTotal += st.cgi.outBuf.size();
            st.cgi.outBuf.clear();
            setPollToWrite(clientFd);
            tryFlushWrite(clientFd, st);
        }
    }

    if (st.cgi.stdout_r == -1)
    {
        bool haveCL = false;
        bool haveTE = false;
        if (!st.cgi.cgiHeaders.empty())
        {
            std::map<std::string,std::string>::const_iterator itH = st.cgi.cgiHeaders.find("content-length");
            if (itH != st.cgi.cgiHeaders.end())
                haveCL = true;
            std::map<std::string,std::string>::const_iterator itTE = st.cgi.cgiHeaders.find("transfer-encoding");
            if (itTE != st.cgi.cgiHeaders.end())
                haveTE = true;
        }
        if (!haveCL && !haveTE)
            st.forceCloseAfterWrite = true;

        if (!clientHasPendingWrite(st))
        {
            setPollToWrite(clientFd);
            tryFlushWrite(clientFd, st);
        }
    }

    reapCgiIfDone(st);
}
