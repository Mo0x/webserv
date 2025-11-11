#include "MultipartStreamParser.hpp"
#include "SocketManager.hpp"
#include <algorithm>
#include "utils.hpp"

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

/*
	Target behaviora:
	Accumulate header lines for the current part until \r\n\r\n.
	Enforce a per-part header cap (e.g., 32–64 KiB).
	Parse lines into m_curHeaders (lowercased keys, trimmed values).
	Fire m_onBegin(m_user, m_curHeaders) once.
	Transition to S_DATA and clear any temporary header buffer.
*/

MultipartStreamParser::HRes MultipartStreamParser::parsePartHeaders(std::string::size_type &outConsumed) // read headers up to CRLFCRLF into m_curHeaders
{
	static const std::string::size_type PART_HEADER_CAP      = 64 * 1024; // 64 KiB
	static const std::string::size_type MAX_PART_HEADER_LINE =  8 * 1024; // 8 KiB
	std::string::size_type p = m_buf.find("\r\n\r\n");
	if (p == std::string::npos)
	{
		if (m_buf.size() > PART_HEADER_CAP)
			return H_ERR;
		else
			return H_MORE;
	}
	std::string block = m_buf.substr(0, p);
	outConsumed = p + 4;
	m_curHeaders.clear();
	/*
	For each non-empty line:
	Find first ':'. If missing → H_ERR.
	name = line[0..colon-1], value = line[colon+1..end].
	Trim leading/trailing spaces of value.
	Lowercase ASCII name (so Content-Disposition → content-disposition).
	Insert into m_curHeaders[name] = value. (If duplicate, last one wins—fine here.)
	*/
	std::string::size_type pos = 0;
	std::string::size_type nl;
	while (pos < block.size())
	{
		nl = block.find("\r\n", pos);
		if (nl == std::string::npos)
			nl = block.size();
		std::string::size_type len = nl - pos;
		if (len > MAX_PART_HEADER_LINE)
			return H_ERR;
		std::string line = block.substr(pos, len);
		pos = (nl == block.size()) ? nl : nl + 2;
		if (line.empty())
			continue;
		std::string::size_type colon = line.find(':');
		if (colon == std::string::npos)
			return H_ERR;
		std::string name = trimCopy(line.substr(0, colon));
		std::string value = trimCopy(line.substr(colon + 1));
		m_curHeaders[toLowerCopy(name)] = value;
	}
	return H_OK;
}

bool MultipartStreamParser::findNextBoundary(std::string::size_type &k, bool &isClosing)
{
	std::string::size_type posClose = m_buf.find("\r\n" + m_delimClose);
	std::string::size_type posReg = m_buf.find("\r\n" + m_delim);
	if (posClose == std::string::npos && posReg == std::string::npos)
		return false;
	if (posClose != std::string::npos && posReg != std::string::npos)
	{
		if (posClose <= posReg)
		{
			k = posClose;
			isClosing = true;
			return true;
		}
		else
		{
			k = posReg;
			isClosing = false;
			return true;
		}
	}
	if (posClose !=std::string::npos)
	{
		k = posClose; 
		isClosing =true;
		return true;
	}
	k = posReg;
	isClosing =false;
	return true;
}

bool MultipartStreamParser::emitDataChunk(size_t upto)
{
	//Call m_onData(m_user, m_buf.data(), upto) (if upto > 0 and m_onData isn’t null).
	//Erase the first upto bytes from m_buf.
	//Return true if it emitted anything (so feed() can set progress = true), false otherwise.
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
				progress = consumeToFirstBoundary();
				break;
			case S_HEADERS :
			{
				std::string::size_type consumed = 0;
				HRes h = parsePartHeaders(consumed);
				if (h == H_MORE)
				{
					// No full CRLFCRLF yet; no progress this iteration.
					break;
				}
				if (h == H_ERR)
				{
					enterError();
					return ERR;
				}
				// H_OK:
				// 1) fire callback exactly once per part
				m_onBegin(m_user, m_curHeaders);

				// 2) drop header block + "\r\n\r\n"
				m_buf.erase(0, consumed);

				// 3) advance
				m_st = S_DATA;
				progress = true;
				break;
			}
			case S_DATA :
			{
			                // find next boundary; emit data before it via m_onData
                // regular boundary -> onEnd(); m_st = S_HEADERS; progress = true;
                // closing boundary -> onEnd(); m_st = S_DONE; return DONE;
                // if partial boundary at tail, keep overlap and break loop
				emitDataChunk(size_t upto);
				break;
			}
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