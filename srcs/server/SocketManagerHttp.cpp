#include "SocketManager.hpp"
#include "Config.hpp"
#include "request_reponse_struct.hpp"
#include "Chunked.hpp"
#include <iostream>

// tryPraseHeader(fd, st)
/* New helper replacing processFirstTimeHeaders(), locateHeaders, enforceHeaderLimits, parseAndValidateRequest it does the following:
		1 check if we have a full header block (\r\n\r\n)
		2 if not we wait (and dont kill the connection)
		3 if we do have it :
				st.isChuncked -? does it have TE:chunked?
				st.contentLength, if not chunked we parse content-length
				st.maxBodyAllowed <- from client_max_body_size
			we strip the header from st.recvBuffer, no it starts with the body's bytes
			decided what's the next phase:
				if the request as a body (post with content-Length, post chunked etc...)
					-> st.phase = READING_BODY
				if it does not (head, delete)
					->st.phase = READY_TO_DISPATCH
		if something is invalid (bad request, disallowed methods, insane header size etc...)
			-> we build an error Response, queue it and set st.phase = SENDING_RESPONSE
			and return false to handleClientRead.
		*/

static void ltrim(std::string &s)
{
	size_t i = 0;
	while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
		i++;
	if (i)
		s.erase (0, i);
}

static void rtrim(std::string &s)
{
	if (s.empty())
		return;
	size_t i = s.size();
	while(i > 0  && (s[i -1] == ' ' || s[i -1] == '\t'))
		i--;
	if (i < s.size())
		s.erase(i);
}

static void trim(std::string &s)
{
	rtrim(s);
	ltrim(s);
}

static void toLowerInPlace(std::string &s)
{
	for (size_t i = 0; i < s.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(s[i]);
		if (c >= 'A' && c <= 'Z')
			s[i] = static_cast<char>(c - 'A' + 'a');
	}
}

static bool isTokenChar(char c)
{
	return ( (c >= 'A' && c <= 'Z')
		|| (c >= 'a' && c <= 'z')
		|| (c >= '0' && c <= '9')
		|| (c == '-') );
}

static bool isValidHeaderName(const std::string &name)
{
	if (name.empty())
		return false;
	for (size_t i = 0; i < name.size(); ++i)
	{
		if (!isTokenChar(name[i]))
			return false;
	}
	return true;
}

static bool findHeaderBoundary(ClientState &st, size_t &hdrEndPos)
{
	hdrEndPos = st.recvBuffer.find("\r\n\r\n");
	if (hdrEndPos == std::string::npos)
		return true;
	hdrEndPos += 4;
	return false;
}

bool SocketManager::checkHeaderLimits(int fd, ClientState &st, size_t &hdrEndPos)
{
	const size_t HEADER_CAP = 32 * 1024;

	if (hdrEndPos > HEADER_CAP)
	{
                Response err = makeHtmlError(431, "Request Header Fields Too Larger", "<h1>431 Request Header Fields Too Larger</h1>");
                finalizeAndQueue(fd, err);
                setPhase(fd, st, ClientState::SENDING_RESPONSE, "checkHeaderLimits");
		return false;
	}

		// TODO (soon): per-line cap (e.g., 8 KiB) and line count cap
	// Iterate st.recvBuffer[0..hdrEndPos) splitting on "\r\n" and check each line.
	return true;
}

bool SocketManager::badRequestAndQueue(int fd, ClientState &st)
{
	Response err = makeHtmlError(400, "Bad Request", "<h1>400 Bad Request</h1>");
        finalizeAndQueue(fd, err);
        setPhase(fd, st, ClientState::SENDING_RESPONSE, "badRequestAndQueue");
	return false;
}

// Split on comma into trimmed, lowercased tokens (drop empties)
static void splitCsvLower(const std::string &s, std::vector<std::string> &out)
{
	out.clear();
	std::string cur;
	for (size_t i = 0; i < s.size(); ++i)
	{
		char c = s[i];
		if (c == ',')
		{
			trim(cur);
			toLowerInPlace(cur);
			if (!cur.empty()) out.push_back(cur);
			cur.clear();
		}
		else
		{
			cur.push_back(c);
		}
	}
	trim(cur);
	toLowerInPlace(cur);
	if (!cur.empty()) out.push_back(cur);
}

/**
 * parseSizeT
 *
 * Purpose:
 *   Parse a non-negative, decimal ASCII integer into a size_t, rejecting anything
 *   that is ambiguous, malformed, or would overflow size_t on this platform.
 *
 * Accepted input:
 *   - One or more digits '0'..'9'
 *   - No leading/trailing spaces or tabs
 *   - No sign ('+' or '-') and no base prefixes ('0x', etc.)
 *
 * Behavior:
 *   - On success: writes the value into `out` and returns true.
 *   - On failure: leaves `out` unspecified and returns false.
 *
 * Overflow handling (the “black magic” preprocessor bit):
 *   - We accumulate into an unsigned long long (`v`) to have as much headroom
 *     as possible while multiplying by 10 and adding the next digit.
 *   - To detect overflow *portably* without compiler builtins, we compare `v`
 *     against the maximum representable value of `size_t`.
 *   - Unfortunately, there is no standard macro for “max size_t as ULL literal”
 *     in C++98, so we do a conservative check guarded by a preprocessor test:
 *
 *       #if ULONG_MAX == 18446744073709551615ULL
 *           if (v > (unsigned long long) (~(size_t)0)) return false;
 *       #endif
 *
 *     Explanation:
 *       - On typical LP64 platforms (Linux/macOS x86_64), `unsigned long` is 64-bit,
 *         so `ULONG_MAX` equals 2^64 - 1, which *also* implies `size_t` is 64-bit.
 *         In that case, we can safely compare `v` to the all-ones bit pattern for size_t,
 *         i.e. `~(size_t)0`. If `v` exceeds that, parsing would overflow `size_t` → reject.
 *       - On 32-bit targets (or other ABIs where `size_t` isn’t 64-bit), this branch
 *         is *not* compiled. That avoids making incorrect assumptions about the width
 *         of `size_t` (and keeps the code C++98-portable).
 *       - Why not use SIZE_MAX? It’s from C99’s <stdint.h>/<cstdint> and not guaranteed
 *         in strict C++98 environments; the above check stays compatible.
 *
 * Edge cases:
 *   - Empty string → false
 *   - Any non-digit character → false
 *   - Very large sequences of digits that don’t fit in size_t → false (caught by check)
 *   - “000123” is accepted (normal decimal semantics)
 *
 * Complexity:
 *   - O(n) over the number of characters; no allocations.
 */

static bool parseSizeT(const std::string &s, size_t &out)
{
	// Decimal non-negative; reject leading +/-, hex, spaces.
	if (s.empty())
		return false;
	unsigned long long v = 0ULL;
	for (size_t i = 0; i < s.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(s[i]);
		if (c < '0' || c > '9')
			return false;
		v = v * 10ULL + static_cast<unsigned long long>(c - '0');
		// clamp to size_t range
#if ULONG_MAX == 18446744073709551615ULL
		if (v > static_cast<unsigned long long>(~static_cast<size_t>(0)))
			return false;
#endif
	}
	out = static_cast<size_t>(v);
	return true;
}

bool SocketManager::parseRawHeadersIntoRequest(int fd, ClientState &st, size_t hdrEndPos)
{
	// we extract just the header
	if (hdrEndPos > st.recvBuffer.size())
		hdrEndPos = st.recvBuffer.size();
	
	std::string block = st.recvBuffer.substr(0, hdrEndPos);
	// we split start line up to \r\n\r\n
	size_t lineEnd = block.find("\r\n");
	if (lineEnd == std::string::npos)
		return badRequestAndQueue(fd, st);
	std::string startLine = block.substr(0, lineEnd);

	// Split into exactly 3 space-separated tokens
	size_t sp1 = startLine.find(' ');
	if (sp1 == std::string::npos)
		return badRequestAndQueue(fd, st);

	size_t sp2 = startLine.find(' ', sp1 + 1);
	if (sp2 == std::string::npos)
		return badRequestAndQueue(fd, st);

	// Ensure there isn't a 4th token (reject extra spaces/tokens)
	if (startLine.find(' ', sp2 + 1) != std::string::npos)
		return badRequestAndQueue(fd, st);

	std::string method = startLine.substr(0, sp1);
	std::string target = startLine.substr(sp1 + 1, sp2 - (sp1 + 1));
	std::string version = startLine.substr(sp2 + 1);

	if (method.empty() || target.empty() || version.empty())
		return badRequestAndQueue(fd, st);

	// Accept only HTTP/1.0 or HTTP/1.1 here (keep it simple)
	if (!(version == "HTTP/1.1" || version == "HTTP/1.0"))
		return badRequestAndQueue(fd, st);

	// Store into st.req (adjust field names to your Request type)
	st.req.method = toUpperCopy(method);
	st.req.path = target;      // or st.req.uri
	st.req.http_version = version;
	std::cerr << "[fd " << fd << "] parsed request line + headers: "
          << st.req.method << " " << st.req.path << " " << st.req.http_version
          << " (hdrs=" << st.req.headers.size() << ")\n";


	// 2) Header fields
	size_t pos = lineEnd + 2; // skip CRLF after start-line
	while (pos < block.size())
	{
		// End of headers: the blank line before CRLFCRLF
		if (block.compare(pos, 2, "\r\n") == 0)
			break;

		size_t nl = block.find("\r\n", pos);
		if (nl == std::string::npos)
			return badRequestAndQueue(fd, st);

		std::string line = block.substr(pos, nl - pos);
		pos = nl + 2;

		// No obs-fold: reject lines starting with SP/HTAB
		if (!line.empty() && (line[0] == ' ' || line[0] == '\t'))
			return badRequestAndQueue(fd, st);

		// Split at first ':'
		size_t colon = line.find(':');
		if (colon == std::string::npos)
			return badRequestAndQueue(fd, st);

		std::string name = line.substr(0, colon);
		std::string value = line.substr(colon + 1);

		trim(name);
		if (!isValidHeaderName(name))
			return badRequestAndQueue(fd, st);

		trim(value); // OWS allowed around the field-value

		// Normalize header name to lowercase for case-insensitive map
		toLowerInPlace(name);

		// Append duplicates with comma-join (simple behavior)
		std::string &slot = st.req.headers[name];
		if (!slot.empty())
			slot.append(",").append(value);
		else
			slot = value;
	}

	return true;

}

bool SocketManager::applyRoutePolicyAfterHeaders(int fd, ClientState &st)
{
	// 1)we grab the active server and the matching route
	const ServerConfig &server = m_serversConfig[m_clientToServerIndex[fd]];
	const RouteConfig  *route  = findMatchingLocation(server, st.req.path);
	// 2) Resovlve max body allowance 
	{
		size_t allowed = 0;
		if (route && route->max_body_size > 0)
			allowed = route->max_body_size;
		else if (server.client_max_body_size > 0)
			allowed = server.client_max_body_size;
		st.maxBodyAllowed = allowed;
	}

	// 2.5) early 501 check (server doesn't implement this verb at all)
	{
		const std::string &m = st.req.method; // expected UPPER already
		if (m != "GET" && m != "POST" && m != "DELETE" && m != "HEAD" && m != "OPTIONS")
		{
			Response res;
			res.status_code = 501;
			res.status_message = "Not Implemented";
			res.headers["Content-Type"] = "text/html; charset=utf-8";
			res.body = "<h1>501 Not Implemented</h1>";
			res.headers["Content-Length"] = to_string(res.body.length());
			res.close_connection = false;

			finalizeAndQueue(fd, res);
			setPhase(fd, st, ClientState::SENDING_RESPONSE, "applyRoutePolicyAfterHeaders");
			return false; // pre-body short-circuit
		}
	}

	
	//3) early 405 check

	if (route && !route->allowed_methods.empty())
	{
		if (!isMethodAllowedForRoute(st.req.method, route->allowed_methods))
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
			res.close_connection = false;

                        finalizeAndQueue(fd, res);
                        setPhase(fd, st, ClientState::SENDING_RESPONSE, "applyRoutePolicyAfterHeaders");
			return false;
		}
	}
	//4) redirect
	if (route && !route->redirect.empty())
	{
		Response redir;
		redir.status_code = 301; // you can switch to 302/307/308 later if you extend config
		redir.status_message = "Moved Permanently";
		redir.headers["Location"] = route->redirect;
		redir.headers["Content-Type"] = "text/html; charset=utf-8";
		redir.body = "<h1>Moved</h1>";
		redir.headers["Content-Length"] = to_string(redir.body.length());
		redir.close_connection = false;

                finalizeAndQueue(fd, redir);
                setPhase(fd, st, ClientState::SENDING_RESPONSE, "applyRoutePolicyAfterHeaders");
		return false;
	}
	//5) If you consider "no route" an error here (older code fell back to server root),
	//    you can keep going and let the dispatcher handle 404. Otherwise, short-circuit now:
	// if (!route) {
	//     Response err = makeHtmlError(404, "Not Found", "<h1>404 Not Found</h1>");
	//     finalizeAndQueue(fd, err);
	//     st.phase = ClientState::SENDING_RESPONSE;
	//     return false;
	// }
	return true;

}



bool SocketManager::setupBodyFramingAndLimits(int fd, ClientState &st)
{
	st.isChunked = false;
	st.contentLength = 0;

	//fetch headers

	std::map<std::string, std::string>::const_iterator itTE  = st.req.headers.find("transfer-encoding");
	std::map<std::string, std::string>::const_iterator itCL  = st.req.headers.find("content-length");

	const bool hasTE = (itTE != st.req.headers.end() && !itTE->second.empty());
	const bool hasCL = (itCL != st.req.headers.end() && !itCL->second.empty());

	//1) has in previous iteration we check if both TE and CL present if so -> 400
	if (hasTE && hasCL)
	{
		Response err = makeHtmlError(400, "Bad Request",
			"<h1>400 Bad Request</h1><p>Both Transfer-Encoding and Content-Length present.</p>");
                finalizeAndQueue(fd, err);
                setPhase(fd, st, ClientState::SENDING_RESPONSE, "setupBodyFramingAndLimits");
		return false;
	}
	// 2) Transfer-Encoding handling (only support 'chunked')
	if (hasTE)
	{
		std::vector<std::string> tokens;
		splitCsvLower(itTE->second, tokens);

		if (tokens.empty())
		{
			Response err = makeHtmlError(400, "Bad Request",
				"<h1>400 Bad Request</h1><p>Empty Transfer-Encoding.</p>");
                        finalizeAndQueue(fd, err);
                        setPhase(fd, st, ClientState::SENDING_RESPONSE, "setupBodyFramingAndLimits");
			return false;
		}

		// Only support exactly "chunked" OR ...,"chunked" as the last step.
		// Be strict: if there are other codings (e.g., gzip), reply 501 for now.
		if (tokens.size() == 1 && tokens[0] == "chunked")
		{
			st.isChunked = true;
			// st.contentLength stays 0
			return true; // no early 413 possible here; enforce during streaming
		}

		// Allow multiple tokens only if last is "chunked" and the rest are identity/empty?
		// To keep it simple and safe, reject multi-codings for now.
		{
			Response err = makeHtmlError(501, "Not Implemented",
				"<h1>501 Not Implemented</h1><p>Unsupported Transfer-Encoding.</p>");
                        finalizeAndQueue(fd, err);
                        setPhase(fd, st, ClientState::SENDING_RESPONSE, "setupBodyFramingAndLimits");
			return false;
		}
	}
	// 3) Content-Length handling
	if (hasCL)
	{
		// If duplicates were merged by parser as "v1,v2", treat as invalid unless exactly one value.
		std::vector<std::string> vals;
		splitCsvLower(itCL->second, vals);
		if (vals.size() != 1)
		{
			Response err = makeHtmlError(400, "Bad Request",
				"<h1>400 Bad Request</h1><p>Multiple Content-Length values.</p>");
                        finalizeAndQueue(fd, err);
                        setPhase(fd, st, ClientState::SENDING_RESPONSE, "setupBodyFramingAndLimits");
			return false;
		}

		// parse decimal
		size_t cl = 0;
		if (!parseSizeT(vals[0], cl))
		{
			Response err = makeHtmlError(400, "Bad Request",
				"<h1>400 Bad Request</h1><p>Invalid Content-Length.</p>");
                        finalizeAndQueue(fd, err);
                        setPhase(fd, st, ClientState::SENDING_RESPONSE, "setupBodyFramingAndLimits");
			return false;
		}

		st.contentLength = cl;

		// 4) Early 413 if CL exceeds policy
		if (st.maxBodyAllowed > 0 && st.contentLength > st.maxBodyAllowed)
		{
			Response err = makeHtmlError(413, "Payload Too Large",
				"<h1>413 Payload Too Large</h1>");
                        finalizeAndQueue(fd, err);
                        setPhase(fd, st, ClientState::SENDING_RESPONSE, "setupBodyFramingAndLimits");
			return false;
		}

		return true;
	}

	// 4) No TE, no CL -> no body expected (for HTTP/1.1 requests without explicit framing)
	// We'll treat it as zero-length body; dispatcher can proceed.
	st.contentLength = 0;
	return true;
}

static const char* phaseToStr(ClientState::Phase p)
{
	switch (p)
	{
		case ClientState::READING_HEADERS:   return "READING_HEADERS";
		case ClientState::READING_BODY:      return "READING_BODY";
		case ClientState::READY_TO_DISPATCH: return "READY_TO_DISPATCH";
		case ClientState::SENDING_RESPONSE:  return "SENDING_RESPONSE";
		case ClientState::CLOSED:            return "CLOSED";
	}
	return "?";
}



// Headers are fully parsed; prepare for body phase or dispatch.

void SocketManager::finalizeHeaderPhaseTransition (int fd, ClientState &st, size_t hdrEndPos)
{
	// 1) Drop the header block from the recv buffer (leave any coalesced body/pipelined data)
	if (hdrEndPos > st.recvBuffer.size())
		hdrEndPos = st.recvBuffer.size(); // defensive
	st.recvBuffer.erase(0, hdrEndPos);
	// 2) Decide the next phase + optionally move already-received body bytes (CL case)
        if (st.isChunked)
        {
                // For chunked, we don't pre-consume here. The chunk decoder will
                // pull from st.recvBuffer during tryReadBody.
                setPhase(fd, st, ClientState::READING_BODY, "finalizeHeaderPhaseTransition");
                return;
        }

	// Non-chunked framing
	if (st.contentLength > 0)
	{
		// How many bytes of the body are already in the recv buffer?
		size_t have = st.recvBuffer.size();
		size_t need = st.contentLength - st.bodyBuffer.size();

                if (have == 0)
                {
                        // No body bytes coalesced yet — switch to body phase and wait for more
                        setPhase(fd, st, ClientState::READING_BODY, "finalizeHeaderPhaseTransition");
                        return;
                }

		// Move as much as we need from recvBuffer -> bodyBuffer
		size_t take = (have < need) ? have : need;
		if (take > 0)
		{
			st.bodyBuffer.append(st.recvBuffer.data(), take);
			st.recvBuffer.erase(0, take);
		}

		// If we finished the body right away, we may already have pipelined data
                if (st.bodyBuffer.size() >= st.contentLength)
                {
                        // Body complete -> ready to dispatch immediately.
                        // Any surplus left in st.recvBuffer is the start of the next request (HTTP pipelining).
                        setPhase(fd, st, ClientState::READY_TO_DISPATCH, "finalizeHeaderPhaseTransition");
                }
                else
                {
                        // Still need more body bytes
                        setPhase(fd, st, ClientState::READING_BODY, "finalizeHeaderPhaseTransition");
                }
        }
        else
        {
                // No body expected (no TE and no CL) — dispatch immediately
                setPhase(fd, st, ClientState::READY_TO_DISPATCH, "finalizeHeaderPhaseTransition");
        }
	std::cerr << "[fd "<< fd << "] header->body: phase=" << phaseToStr(st.phase)
          << " recv=" << st.recvBuffer.size()
          << " body=" << st.bodyBuffer.size() << std::endl;
}

bool SocketManager::doTheMultiPartThing(int fd, ClientState &st)
{
	//1) we only act for request that have bodies
	if (st.req.method != "POST")

	return false;
}

bool SocketManager::tryParseHeaders(int fd, ClientState &st)
{
		// 1) do we have full headers?
	size_t hdrEndPos;
	std::cerr << "[fd " << fd << "] tryReadBody: chunked=" << (st.isChunked?1:0)
          << " recv=" << st.recvBuffer.size()
          << " body=" << st.bodyBuffer.size() << std::endl;
	if (findHeaderBoundary(st, hdrEndPos))
		return true;

	// 2) limits
	if (!checkHeaderLimits(fd, st, hdrEndPos))
		return false;

	// 3) parse start-line + headers (fills st.req.*, lowercases header names)
	if (!parseRawHeadersIntoRequest(fd, st, hdrEndPos))
		return false;

	// (optional) header dump for debugging
	std::cerr << "[fd " << fd << "] headers:";
	for (std::map<std::string,std::string>::const_iterator it = st.req.headers.begin();
		it != st.req.headers.end(); ++it)
		std::cerr << " [" << it->first << ": " << it->second << "]";
	std::cerr << std::endl;

	// 4) policy (method allowed, maxBodyAllowed, redirects...)
	if (!applyRoutePolicyAfterHeaders(fd, st))
		return false;

	// 5) framing (sets st.isChunked / st.contentLength, may queue 4xx/5xx)
	if (!setupBodyFramingAndLimits(fd, st))
		return false;

	// 6) MultiPart detection and init

	if (!doTheMultiPartThing(fd, st))
		return false;
	std::cerr << "[fd " << fd << "] framing: isChunked="
			<< (st.isChunked?1:0) << " contentLength=" << st.contentLength << std::endl;

	// 6) transition (only enter READING_BODY if we actually have framing)
	finalizeHeaderPhaseTransition(fd, st, hdrEndPos);

	return true;
}