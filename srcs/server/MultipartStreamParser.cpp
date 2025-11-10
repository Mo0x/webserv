#include "MultipartStreamParser.hpp"
#include "SocketManager.hpp"
#include <algorithm>

MultipartStreamParser::MultipartStreamParser() :
	m_st(MultipartStreamParser::S_ERROR),
	m_onBegin(NULL),
	m_onData(NULL),
	m_onEnd(NULL),
	m_user(NULL)
{
	return ;
}

// we accept only [A-Za-z0-9._+-]{1,70}

static bool isValidBoundary(const std::string &boundary)
{
	for (size_t i = 0; i < boundary.size(); i++)
	{
		if (!((boundary[i] >= 'a' && boundary[i]<= 'z') ||
		(boundary[i] >= 'A' && boundary[i] <= 'Z') ||
		(boundary[i] >= '0' && boundary[i] <= '9') ||
		boundary [i] == '.' || boundary[i] == '_' ||
		boundary[i] == '+' || boundary[i] == '-'))
		return false;
	}
	return true;
}

/*
Take a boundary and callbacks for this specific HTTP request.
Reinitialize the parser so the next feed() starts from a clean slate at S_PREAMBLE.
Precompute delimiter strings to make scanning fast.
Clear rolling buffers and per-part state from any previous request.
*/

void MultipartStreamParser::reset(	const std::string &boundary,
									PartBeginCb onBegin,
									PartDataCb onData, 
									PartEndCb onEnd, 
									void *user)
{
	if (boundary.empty() || boundary.size() > 70 || !isValidBoundary(boundary) || !onBegin || !onEnd)
	{
		m_st = S_ERROR;
		return ;
	}
	m_boundary = boundary;
	m_user = user;
	m_onBegin = onBegin;
	m_onData = onData;
	m_onEnd = onEnd;

	m_delim = std::string("--") + boundary;
	m_delimClose = std::string("--") + boundary + std::string("--");

	m_buf.clear();
	m_curHeaders.clear();

	m_st = S_PREAMBLE;
}

bool MultipartStreamParser::consumeToFirstBoundary()
{
	// Max possible delimiter line: "--" + boundary + "--" + "\r\n"  => size() + 6
	const std::string::size_type overlapLen =
		(std::string::size_type)std::max<size_t>(m_boundary.size() + 6, 256u);

	const std::string::size_type kReg   = m_buf.find(m_delim);       // "--" + boundary
	const std::string::size_type kClose = m_buf.find(m_delimClose);  // "--" + boundary + "--"

	// Nothing found yet: trim safe prefix, keep only overlap tail
	if (kReg == std::string::npos && kClose == std::string::npos)
	{
		if (m_buf.size() > overlapLen)
			m_buf.erase(0, m_buf.size() - (overlapLen - 1));
		return false;
	}

	// Choose earliest; if same index, disambiguate by looking for "--" right after m_delim
	std::string::size_type k;
	enum { REGULAR = 1, CLOSING = 2 };
	int kind;

	if (kReg != std::string::npos && (kClose == std::string::npos || kReg < kClose))
	{
		k = kReg;
		kind = REGULAR;
	}
	else if (kClose != std::string::npos && (kReg == std::string::npos || kClose < kReg))
	{
		k = kClose;
		kind = CLOSING;
	}
	else /* kReg == kClose (same index) */
	{
		// Need 2 bytes to disambiguate (the "--" after regular delimiter means closing)
		const std::string::size_type needPeek = kReg + m_delim.size() + 2;
		if (needPeek > m_buf.size())
		{
			// Partial delimiter split across feeds; keep overlap and wait for more
			if (m_buf.size() > overlapLen)
				m_buf.erase(0, m_buf.size() - (overlapLen - 1));
			return false;
		}
		const std::string after = m_buf.substr(kReg + m_delim.size(), 2);
		if (after == "--")
		{
			k = kClose;
			kind = CLOSING;
		}
		else
		{
			k = kReg;
			kind = REGULAR;
		}
	}

	// Ensure full delimiter line (with CRLF) is present
	std::string::size_type endOfLine;
	if (kind == REGULAR)
	{
		const std::string::size_type need = m_delim.size() + 2; // + "\r\n"
		if (k + need > m_buf.size())
		{
			if (m_buf.size() > overlapLen)
				m_buf.erase(0, m_buf.size() - (overlapLen - 1));
			return false;
		}
		if (m_buf.compare(k + m_delim.size(), 2, "\r\n") != 0)
		{
			enterError();
			return true;
		}
		endOfLine = k + need;
	}
	else // CLOSING
	{
		const std::string::size_type need = m_delimClose.size() + 2; // + "\r\n"
		if (k + need > m_buf.size())
		{
			if (m_buf.size() > overlapLen)
				m_buf.erase(0, m_buf.size() - (overlapLen - 1));
			return false;
		}
		if (m_buf.compare(k + m_delimClose.size(), 2, "\r\n") != 0)
		{
			enterError();
			return true;
		}
		endOfLine = k + need;
	}

	// Consume preamble + the whole delimiter line (including trailing CRLF)
	m_buf.erase(0, endOfLine);

	// Advance state
	if (kind == CLOSING)
		m_st = S_DONE;
	else
		m_st = S_HEADERS;

	return true; // progress made
}

MultipartStreamParser::Result MultipartStreamParser::feed(const char* data, size_t n)
{
	if (m_st == S_DONE)
		return DONE;
	if (m_st == S_ERROR)
		return ERR;
	if (n)
		m_buf.append(data, n);
	bool progress = true;
	while (progress)
	{
		progress = false;
		switch(m_st)
		{
			case S_PREAMBLE :
			/*helper that will scan first boundary, if we consume anything of advance state, progress = true*/
				break;
			case S_HEADERS :
			// if parsePartHeaders() found CRLFCRLF within cap:
			//   m_onBegin(...); m_st = S_DATA; progress = true;
			// else if cap exceeded -> enterError() and return ERR;
				break;
			case S_DATA :
			                // find next boundary; emit data before it via m_onData
                // regular boundary -> onEnd(); m_st = S_HEADERS; progress = true;
                // closing boundary -> onEnd(); m_st = S_DONE; return DONE;
                // if partial boundary at tail, keep overlap and break loop
				break;
			case S_DONE :
				return DONE;
				break;
			case S_ERROR :
				return ERR;
				break;
			default :
				break;
		}
	}
	return MORE;
}