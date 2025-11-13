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

    if (st.cgi.stdin_w != pipefd || st.cgi.stdin_closed) {
        // defensive
        delPollFd(pipefd);
        m_cgiStdinToClient.erase(it);
        return;
    }

    // write as much of inBuf as possible
    if (!st.cgi.inBuf.empty()) {
        ssize_t n = ::write(pipefd, &st.cgi.inBuf[0], st.cgi.inBuf.size());
        if (n > 0) {
            st.cgi.inBuf.erase(0, static_cast<size_t>(n));
            st.cgi.bytesInTotal += static_cast<size_t>(n);
        } else if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                // keep POLLOUT
                return;
            }
            // hard error -> close stdin, keep reading stdout
        }
    }

    if (st.cgi.inBuf.empty()) {
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
    for (;;) {
        ssize_t n = ::read(pipefd, buf, sizeof(buf));
        if (n > 0) {
            st.cgi.outBuf.append(buf, static_cast<size_t>(n));
            st.cgi.bytesOutTotal += static_cast<size_t>(n);
        } else if (n == 0) {
            // EOF on CGI stdout
            ::close(pipefd);
            st.cgi.stdout_r = -1;
            delPollFd(pipefd);
            m_cgiStdoutToClient.erase(it);
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
            // hard error -> close pipe, stop polling
            ::close(pipefd);
            st.cgi.stdout_r = -1;
            delPollFd(pipefd);
            m_cgiStdoutToClient.erase(it);
            break;
        }
    }

    // Defer parsing and HTTP emission to a dedicated step:
    // We'll add a "drainCgiOutput()" that is called from handleClientRead()
    // or right after this to parse headers once and start streaming to writeBuffer.
    // (We'll do that in the next step when you’re ready.)
}

void SocketManager::handleCgiPipeError(int pipefd)
{
	if (isCgiStdout(pipefd)) {
		int clientFd = m_cgiStdoutToClient[pipefd];
		delPollFd(pipefd); ::close(pipefd);
		m_cgiStdoutToClient.erase(pipefd);
		m_clients[clientFd].cgi.stdout_r = -1;
	}
	if (isCgiStdin(pipefd)) {
		int clientFd = m_cgiStdinToClient[pipefd];
		delPollFd(pipefd); ::close(pipefd);
		m_cgiStdinToClient.erase(pipefd);
		m_clients[clientFd].cgi.stdin_w = -1;
		m_clients[clientFd].cgi.stdin_closed = true;
	}
}
