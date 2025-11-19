#include "SocketManager.hpp"
#include "Config.hpp"
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream> // for std::ostringstream
#include <cstdlib>
#include <cerrno>
#include <cstring> //for std::strerror
#include <cctype>
#include <iostream>
#include <signal.h> //kill
#include <sys/wait.h>

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
    if (it == m_cgiStdinToClient.end()) return;
    int clientFd = it->second;
    ClientState &st = m_clients[clientFd];

    if (st.cgi.stdin_w != pipefd || st.cgi.stdin_closed) 
    {
        // defensive
        delPollFd(pipefd);
        m_cgiStdinToClient.erase(it);
        return;
    }

    // write as much of inBuf as possible
    if (!st.cgi.inBuf.empty()) 
    {
        ssize_t n = ::write(pipefd, &st.cgi.inBuf[0], st.cgi.inBuf.size());
        if (n > 0) 
        {
            st.cgi.inBuf.erase(0, static_cast<size_t>(n));
            st.cgi.bytesInTotal += static_cast<size_t>(n);
        } 
        else if (n < 0) 
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) 
            {
                // keep POLLOUT
                return;
            }
            // hard error -> close stdin, keep reading stdout
        }
    }

    if (st.cgi.inBuf.empty()) 
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
    if (st.cgi.pid > 0)
    {
        int status = 0;
        (void) waitpid(st.cgi.pid, &status, WNOHANG);
    }
}

static void inject_header_lines_before_body(std::string &respBuf,
                                            const std::vector<std::string> &lines)
{
    if (lines.empty()) return;
    size_t p = respBuf.find("\r\n\r\n");
    size_t adv = 4;
    if (p == std::string::npos) {
        p = respBuf.find("\n\n");
        adv = 2;
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

bool SocketManager::parseCgiHeaders(ClientState &st, int clientFd, const RouteConfig &route)
{
    const size_t CGI_HEADER_CAP = 32 * 1024;

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
        key = to_lower_copy(key);

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

    // Determine HTTP status
    int status = 200;
    std::map<std::string,std::string>::const_iterator itS = merged.find("status");
    if (itS != merged.end()) {
        const std::string &sv = itS->second;
        int n = 0;
        if (!sv.empty() && std::isdigit((unsigned char)sv[0])) n = std::atoi(sv.c_str());
        if (n >= 100 && n <= 599) status = n;
    } else if (merged.find("location") != merged.end()) {
        status = 302; // Location without Status → redirect
    }
    st.cgi.cgiStatus = status;

    // Build upstream response headers once
    Response res;
    res.status_code = status;
    res.status_message = ""; // optional
    // forward selected headers (filter hop-by-hop)
    for (std::map<std::string,std::string>::const_iterator it = merged.begin();
         it != merged.end(); ++it)
    {
        const std::string &k = it->first;
        const std::string &v = it->second;
        if (k == "status") continue;
        if (k == "connection" || k == "transfer-encoding" ||
            k == "keep-alive" || k == "proxy-connection" || k == "upgrade")
            continue;

        if (k == "content-type")      res.headers["Content-Type"]      = v;
        else if (k == "location")     res.headers["Location"]          = v;
        else if (k == "content-length") res.headers["Content-Length"]  = v; // allow keep-alive if present
        else if (k == "cache-control")  res.headers["Cache-Control"]   = v;
        else if (k == "expires")        res.headers["Expires"]         = v;
        else if (k == "pragma")         res.headers["Pragma"]          = v;
        else if (k == "last-modified")  res.headers["Last-Modified"]   = v;
        else if (k == "etag")           res.headers["ETag"]            = v;
        else if (k == "vary")           res.headers["Vary"]            = v;
        else if (k == "content-language") res.headers["Content-Language"] = v;
        // ignore unknowns or add more as needed
    }

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
    if (itc == m_clients.end()) return;
    ClientState &st = itc->second;

    if (st.phase != ClientState::CGI_RUNNING && !clientHasPendingWrite(st)) {
        return; // nothing to do
    }

    const ServerConfig &srv = m_serversConfig[m_clientToServerIndex[clientFd]];
    const RouteConfig  *rt  = findMatchingLocation(srv, st.req.path);
    const size_t timeout_ms = (rt ? rt->cgi_timeout_ms : 0);
    const size_t max_bytes  = (rt ? rt->cgi_max_output_bytes : 0);

    // Timeout
    if (timeout_ms > 0 &&
        (now_ms() - st.cgi.tStartMs) > (unsigned long long)timeout_ms)
    {
        killCgiProcess(st, SIGKILL);

        if (!st.cgi.headersParsed) {
            Response err = makeHtmlError(504, "Gateway Timeout", "<h1>504 Gateway Timeout</h1>");
            finalizeAndQueue(clientFd, st.req, err, false, true);
        } else {
            st.forceCloseAfterWrite = true; // end body by connection close
        }

        if (st.cgi.stdin_w  != -1) { ::close(st.cgi.stdin_w);  st.cgi.stdin_w  = -1; }
        if (st.cgi.stdout_r != -1) { ::close(st.cgi.stdout_r); st.cgi.stdout_r = -1; }
        reapCgiIfDone(st);
        setPollToWrite(clientFd);
        tryFlushWrite(clientFd, st);
        return;
    }

    // If headers not sent yet, try to parse them
    if (!st.cgi.headersParsed) {
        if (rt && !st.cgi.outBuf.empty()) {
            if (!parseCgiHeaders(st, clientFd, *rt)) {
                // need more bytes for headers — just wait
            } else {
                // After queuing headers, phase is SENDING_RESPONSE.
                setPollToWrite(clientFd);
                tryFlushWrite(clientFd, st);
            }
        }
    }

    // Stream body if we have headers and output
    if (st.cgi.headersParsed && !st.cgi.outBuf.empty()) {
        // Enforce output cap (on forwarded body)
        if (max_bytes > 0 &&
            st.cgi.bytesOutTotal + st.cgi.outBuf.size() > max_bytes)
        {
            killCgiProcess(st, SIGKILL);
            st.forceCloseAfterWrite = true; // end by close after flush
            if (st.cgi.stdin_w  != -1) { ::close(st.cgi.stdin_w);  st.cgi.stdin_w  = -1; }
            if (st.cgi.stdout_r != -1) { ::close(st.cgi.stdout_r); st.cgi.stdout_r = -1; }
            reapCgiIfDone(st);
            // Optionally truncate to exactly max_bytes; simplest is to stop forwarding more.
            st.cgi.outBuf.clear();
        }
        else
        {
            if (st.req.method == "HEAD") {
                // Drain but DO NOT forward body for HEAD
                st.cgi.bytesOutTotal += st.cgi.outBuf.size();
                st.cgi.outBuf.clear();
            } else {
                st.writeBuffer.append(st.cgi.outBuf);
                st.cgi.bytesOutTotal += st.cgi.outBuf.size();
                st.cgi.outBuf.clear();
                setPollToWrite(clientFd);
                tryFlushWrite(clientFd, st);
            }
        }
    }

    // If CGI stdout closed and nothing left to flush → finalize connection choice
    if (st.cgi.stdout_r == -1 && !clientHasPendingWrite(st)) {
        // If CGI did not provide Content-Length, we must end body by closing.
        bool haveCL = false;
        if (!st.cgi.cgiHeaders.empty()) {
            std::map<std::string,std::string>::const_iterator itH = st.cgi.cgiHeaders.find("content-length");
            haveCL = (itH != st.cgi.cgiHeaders.end());
        }
        if (!haveCL) {
            st.forceCloseAfterWrite = true; // signal end-of-body via connection close
        }
        reapCgiIfDone(st);
        setPollToWrite(clientFd);
        tryFlushWrite(clientFd, st);
    }
}