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
		st.phase = ClientState::SENDING_RESPONSE;
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
	st.phase = ClientState::SENDING_RESPONSE;
	return false;
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

	// Normalize method to uppercase (ASCII)
	for (size_t i = 0; i < method.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(method[i]);
		if (c >= 'a' && c <= 'z')
			method[i] = static_cast<char>(c - 'a' + 'A');
	}

	// Accept only HTTP/1.0 or HTTP/1.1 here (keep it simple)
	if (!(version == "HTTP/1.1" || version == "HTTP/1.0"))
		return badRequestAndQueue(fd, st);

	// Store into st.req (adjust field names to your Request type)
	st.req.method = method;
	st.req.path = target;      // or st.req.uri
	st.req.http_version = version;

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

bool SocketManager::tryParseHeader(int fd, ClientState &st)
{
	// 1) do we have header yet
	size_t hdrEndPos; 
	if (!findHeaderBoundary(st, hdrEndPos))
		return true;
	// 2) check header sanity /size limits
	if (!checkHeaderLimits(fd, st, hdrEndPos))
		return false; // we already queued an error reponse or killed the connection

	// 3) Parse the request line +headers;
	if (!parseRawHeadersIntoRequest(fd, st, hdrEndPos))
		return false;
	// 4..6) applyRoutePolicyAfterHeaders, setupBodyFramingAndLimits,
	//       finalizeHeaderPhaseTransition
	// (call them in order; return false only if you queued an error)
	return true;

}