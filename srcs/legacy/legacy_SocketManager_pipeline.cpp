// LEGACY: retained for reference; not used in current pipeline.
// Original location: srcs/server/SocketManager.cpp (header/body parsing helpers)
// Last moved: 2025-02-15

#include "SocketManager.hpp"
#include <iostream>

// This translation unit preserves the previous header/body processing pipeline
// that has since been superseded by SocketManagerHttp.cpp. To re-enable, add this
// file to the build and ensure the declarations in SocketManager.hpp remain.

static int validateBodyFraming(const std::map<std::string, std::string> &headers,
                                                        size_t &outContentLength,
                                                        bool &outHasTE)
{
        outContentLength = 0;
        outHasTE = false;
        size_t clCount = 0;
        std::string clValue;

        for (std::map<std::string , std::string>::const_iterator it = headers.begin(); it != headers.end(); it++)
        {
                const std::string &key = it->first;
                const std::string &value = it->second;
                if (key == "content-length")
                {
                        ++clCount;
                        clValue = value;
                }
                else if (key == "transfer-encoding")
                {
                        outHasTE = true;
                }
        }
        if (clCount > 1)
                return 400;
        if (clCount == 1 && outHasTE)
                return 400;
        // if CL present, verify digits-only and then parse into size_t safely
        if (clCount == 1)
        {
                if (clValue.empty())
                        return 400;
                for (size_t i = 0; i < clValue.size(); ++i)
                {
                        const unsigned char ch = static_cast<unsigned char>(clValue[i]);
                        if (ch < '0' || ch > '9')
                                return 400;
                }

                /* here we parse with a guard against overflow !
                Let’s say size_t is 64 bits, so its max is 18446744073709551615.

                MAX_DIV10 = the largest integer X such that X * 10 still fits in size_t.
                → here it’s 1844674407370955161 (we drop the last digit 5).

                MAX_LAST = the remaining “last safe digit” (the remainder of max ÷ 10)
                → here it’s 5.

                EG:
                Say size_t max = 999.

                Then MAX_DIV10 = 99 and MAX_LAST = 9.

                Now parsing "1000":

                step    val     digit   check
                '1'             0       1               ok → val=1
                '0'             1       0               ok → val=10
                '0'             10      0               ok → val=100
                '0'             100     0               val(100) > MAX_DIV10(99) → reject 400
                */
                size_t val = 0;
                const size_t MAX_DIV10 = static_cast<size_t>(-1) / 10;
                const size_t MAX_LAST = static_cast<size_t>(-1) % 10;

                for (size_t i = 0; i < clValue.size(); ++i)
                {
                        size_t digit = static_cast<size_t>(clValue[i] - '0'); // classic ascii trick
                        if (val > MAX_DIV10 || (val == MAX_DIV10 && digit > MAX_LAST))
                                return 400;
                        val = val * 10 + digit;
                }
                outContentLength = val;
        }
        return 0; // means OK here
}

bool SocketManager::locateHeaders(int fd, size_t &hdrEnd)
{
        // ---- header byte cap pre-check (before we have full headers) -----------
        const size_t HEADER_CAP = 32 * 1024;
        if (m_clientBuffers[fd].size() > HEADER_CAP &&
                m_clientBuffers[fd].find("\r\n\r\n") == std::string::npos)
        {
                queueErrorAndClose(fd, 431, "Request Header Fields Too Large",
                                                   "<h1>431 Request Header Fields Too Large</h1>");
                return false;
        }

        // ---- locate end of headers ---------------------------------------------
        hdrEnd = m_clientBuffers[fd].find("\r\n\r\n");
        if (hdrEnd == std::string::npos)
                return false; // keep reading headers

        return true;
}

bool SocketManager::enforceHeaderLimits(int fd, size_t hdrEnd)
{
        // ---- post-parse header size caps (to catch huge single headers) ---------
        const size_t HEADER_CAP = 32 * 1024;
        const size_t MAX_HEADER_LINE = 8 * 1024;  // per-line limit

        // total header block size (up to but not including the blank line)
        if (hdrEnd > HEADER_CAP)
        {
                queueErrorAndClose(fd, 431, "Request Header Fields Too Large",
                                                   "<h1>431 Request Header Fields Too Large</h1>");
                return false;
        }

        // per-line length cap
        size_t pos = 0;
        while (pos < hdrEnd)
        {
                size_t nl = m_clientBuffers[fd].find("\r\n", pos);
                if (nl == std::string::npos || nl > hdrEnd)
                        break;
                if (nl - pos > MAX_HEADER_LINE)
                {
                        queueErrorAndClose(fd, 431, "Request Header Fields Too Large",
                                                           "<h1>431 Request Header Fields Too Large</h1>");
                        return false;
                }
                pos = nl + 2;
        }

        // ---- header line-count cap (after headers complete) ---------------------
        const size_t MAX_HEADER_LINES = 100;
        size_t lines = 0;
        pos = 0;
        while (pos < hdrEnd)
        {
                size_t nl = m_clientBuffers[fd].find("\r\n", pos);
                if (nl == std::string::npos || nl > hdrEnd)
                        break;
                ++lines;
                if (lines > MAX_HEADER_LINES)
                {
                        queueErrorAndClose(fd, 431, "Request Header Fields Too Large",
                                                           "<h1>431 Request Header Fields Too Large</h1>");
                        return false;
                }
                pos = nl + 2;
        }

        // ---- duplicate/conflict checks from RAW header block (pre-parse) --------
        const std::string headerBlock = m_clientBuffers[fd].substr(0, hdrEnd);
        std::map<std::string, size_t> nameCounts;
        countHeaderNames(headerBlock, nameCounts);
        if (nameCounts["content-length"] > 1)
        {
                queueErrorAndClose(fd, 400, "Bad Request",
                                                   "<h1>400 Bad Request</h1><p>Multiple Content-Length headers</p>");
                return false;
        }
        if (nameCounts["content-length"] >= 1 && nameCounts["transfer-encoding"] >= 1)
        {
                queueErrorAndClose(fd, 400, "Bad Request",
                                                   "<h1>400 Bad Request</h1><p>Both Content-Length and Transfer-Encoding present</p>");
                return false;
        }

        return true;
}

bool SocketManager::parseAndValidateRequest(int fd, size_t hdrEnd, Request &req,
                                                                                        const ServerConfig* &server,
                                                                                        std::string &methodUpper,
                                                                                        size_t &contentLength,
                                                                                        bool &hasTE)
{
        // ---- parse request line + headers once ----------------------------------
        req = parseRequest(m_clientBuffers[fd].substr(0, hdrEnd + 4));

        // normalize header keys BEFORE any framing/CL/TE validation
        normalizeHeaderKeys(req.headers);

        methodUpper = toUpperCopy(req.method);

        // Safe server mapping (non-throwing path shown; use your helper/try-version if you have it)
        const ServerConfig* serverPtr = 0;
        try
        {
                serverPtr = &findServerForClient(fd);
        }
        catch (...)
        {
                queueErrorAndClose(fd, 400, "Bad Request",
                                                   "<h1>400 Bad Request</h1><p>No server mapping for this connection</p>");
                return false;
        }
        server = serverPtr;

        // ---- numeric CL/TE validation (overflow-safe) ---------------------------
        contentLength = 0;
        hasTE = false;
        int framingErr = validateBodyFraming(req.headers, contentLength, hasTE);
        if (framingErr != 0)
        {
                queueErrorAndClose(fd, 400, "Bad Request",
                                                   "<h1>400 Bad Request</h1><p>Invalid Content-Length / Transfer-Encoding</p>");
                return false;
        }

        if (hasTE)
        {
                std::map<std::string, std::string>::const_iterator itTE = req.headers.find("transfer-encoding");
                if (itTE != req.headers.end())
                {
                        std::string teVal = toLowerCopy(itTE->second);
                        // trim spaces
                        while (!teVal.empty() && (teVal[0] == ' ' || teVal[0] == '\t'))
                                teVal.erase(0, 1);
                        while (!teVal.empty() && (teVal[teVal.size() - 1] == ' ' || teVal[teVal.size() - 1] == '\t'))
                                teVal.erase(teVal.size() - 1);

                        // Only "chunked" is supported, no stacked encodings
                        if (teVal != "chunked")
                        {
                                queueErrorAndClose(fd, 400, "Bad Request",
                                                                   "<h1>400 Bad Request</h1><p>Unsupported Transfer-Encoding</p>");
                                return false;
                        }
                }
        }

        // ---- policy: client_max_body_size (skip when chunked) -------------------
        if (!hasTE && server->client_max_body_size > 0 && contentLength > server->client_max_body_size)
        {
                queueErrorAndClose(fd, 413, "Payload Too Large",
                                                   "<h1>413 Payload Too Large</h1>");
                return false;
        }

        return true;
}

bool SocketManager::processFirstTimeHeaders(int fd, const Request &req,
                                                                                        const ServerConfig &server,
                                                                                        const std::string &methodUpper,
                                                                                        bool hasTE,
                                                                                        size_t contentLength)
{
        // ---- first-time per-FD header processing (limits, 405/411, etc.) --------
        if (m_headersDone[fd])
                return true;

        m_headersDone[fd] = true;

        const RouteConfig* route = findMatchingLocation(server, req.path);

        // resolve per-FD allowed max body (route overrides server)
        size_t allowed = 0;
        if (route && route->max_body_size > 0)
                allowed = route->max_body_size;
        else if (server.client_max_body_size > 0)
                allowed = server.client_max_body_size;
        m_allowedMaxBody[fd] = allowed;

        // early 501 (pre-body) — server doesn't implement this method at all
        if (methodUpper != "GET" && methodUpper != "POST" &&
                methodUpper != "DELETE" && methodUpper != "HEAD" &&
                methodUpper != "OPTIONS")
        {
                Response res;
                res.status_code = 501;
                res.status_message = "Not Implemented";
                res.headers["Content-Type"] = "text/html; charset=utf-8";
                res.body = "<h1>501 Not Implemented</h1>";
                res.headers["Content-Length"] = to_string(res.body.length());
                finalizeAndQueue(fd, res);
                return false;
        }

        // early 405 (pre-body)
        if (route && !route->allowed_methods.empty() &&
                !isMethodAllowedForRoute(methodUpper, route->allowed_methods))
        {
                std::set<std::string> allowSet = normalizeAllowedForAllowHeader(route->allowed_methods);
                std::string allowHeader = joinAllowedMethods(allowSet);

                Response res;
                res.status_code = 405;
                res.status_message = "Method Not Allowed";
                res.headers["Allow"] = allowHeader;
                res.headers["Content-Type"] = "text/html; charset=utf-8";
                res.body = "<h1>405 Method Not Allowed</h1>";
                res.headers["Content-Length"] = to_string(res.body.length());
                finalizeAndQueue(fd, res);
                return false;
        }

        // expectations from validated framing
        m_expectedContentLen[fd] = (!hasTE ? contentLength : 0);
        m_isChunked[fd] = hasTE;

        // 411: POST/PUT must provide length (CL or chunked)
        if ((methodUpper == "POST" || methodUpper == "PUT") &&
                !m_isChunked[fd] && m_expectedContentLen[fd] == 0)
        {
                Response res;
                res.status_code = 411;
                res.status_message = "Length Required";
                res.headers["Content-Type"] = "text/html; charset=utf-8";
                res.body = "<h1>411 Length Required</h1>";
                res.headers["Content-Length"] = to_string(res.body.length());
                finalizeAndQueue(fd, res);
                return false;
        }

        std::map<std::string, std::string>::const_iterator itExp = req.headers.find("expect");
        if (itExp != req.headers.end())
        {
                // Only HTTP/1.1 defines Expect: 100-continue semantics; ignore on HTTP/1.0.
                const bool isHTTP11 = (req.http_version.size() >= 8 &&
                                                           req.http_version.compare(0, 8, "HTTP/1.1") == 0);

                if (isHTTP11)
                {
                        // Normalize the value; if there are multiple expectations, keep first token.
                        std::string expectVal = toLowerCopy(itExp->second);
                        size_t comma = expectVal.find(',');
                        if (comma != std::string::npos)
                                expectVal.erase(comma);
                        // trim simple spaces/tabs
                        while (!expectVal.empty() && (expectVal[0] == ' ' || expectVal[0] == '\t'))
                                expectVal.erase(0, 1);
                        while (!expectVal.empty() && (expectVal[expectVal.size() - 1] == ' ' || expectVal[expectVal.size() - 1] == '\t'))
                                expectVal.erase(expectVal.size() - 1);

                        // RFC: we only recognize token "100-continue". Anything else -> 417.
                        if (expectVal == "100-continue" || !expectVal.empty())
                        {
                                Response res;
                                res.status_code = 417;                      // Expectation Failed
                                res.status_message = "Expectation Failed";
                                res.headers["content-type"] = "text/plain; charset=utf-8";
                                res.body = "417 Expectation Failed\n";
                                res.headers["Content-Length"] = to_string(res.body.length());

                                // No body is (or will be) consumed here.
                                std::cerr << "[EXPECT] 417 on fd " << fd
                                                  << " http=" << req.http_version
                                                  << " expect='" << itExp->second << "'\n";

                                finalizeAndQueue(fd, res);
                                return false; // stop processing this request
                        }
                        // (If value empty, fall through and ignore — extremely unlikely in practice.)
                }
                // HTTP/1.0: ignore Expect entirely.
        }

        return true;
}

bool SocketManager::ensureBodyReady(int fd, size_t hdrEnd, size_t &requestEnd)
{
        // ---- body accounting / streaming limits ---------------------------------
        size_t bodyStart = hdrEnd + 4;
        size_t bodyBytes = (m_clientBuffers[fd].size() > bodyStart)
                                           ? (m_clientBuffers[fd].size() - bodyStart) : 0;

        requestEnd = bodyStart; // default when no body is expected

        // 413 streaming cap
        if (m_allowedMaxBody[fd] > 0 && bodyBytes > m_allowedMaxBody[fd])
        {
                queueErrorAndClose(fd, 413, "Payload Too Large",
                                                   "<h1>413 Payload Too Large</h1>");
                return false;
        }

        // 400 if body exceeds declared Content-Length
        if (m_expectedContentLen[fd] > 0 && bodyBytes > m_expectedContentLen[fd])
        {
                queueErrorAndClose(fd, 400, "Bad Request",
                                                   "<h1>400 Bad Request</h1><p>Body exceeds Content-Length</p>");
                return false;
        }

        // wait for full CL body (if any)
        if (m_expectedContentLen[fd] > 0 && bodyBytes < m_expectedContentLen[fd])
                return false;
        if (m_expectedContentLen[fd] > 0)
                requestEnd = bodyStart + m_expectedContentLen[fd];

        // wait for chunked terminator (placeholder until real decoder)
        if (m_isChunked[fd])
        {
                size_t endMarkerPos = std::string::npos;
                if (m_clientBuffers[fd].size() >= bodyStart + 4)
                        endMarkerPos = m_clientBuffers[fd].find("0\r\n\r\n", bodyStart);
                if (endMarkerPos == std::string::npos)
                        return false;
                requestEnd = endMarkerPos + 5; // length of "0\r\n\r\n"
        }

        return true;
}
