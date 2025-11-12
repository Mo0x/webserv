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

void MultipartStreamParser::enterError()
{
	m_st = S_ERROR;
	m_buf.clear();
	m_curHeaders.clear();
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
	if (upto == 0)
		return false;
	if (upto > m_buf.size())
		upto = m_buf.size();
	if (m_onData)
		m_onData(m_user, m_buf.data(), static_cast<size_t>(upto));
	m_buf.erase(0, upto);
	return true;
}

MultipartStreamParser::DRes MultipartStreamParser::s_dataFlow(bool &progress)
{
	progress = false;

	// 1) Find the next boundary (must be preceded by CRLF in S_DATA)
	std::string::size_type k = 0;
	bool isClosing = false;
	if (!findNextBoundary(k, isClosing))
	{
		// No boundary visible yet → emit safe payload except a tail overlap
		const std::string::size_type overlap =
			static_cast<std::string::size_type>(std::max<size_t>(m_boundary.size() + 8, 256u)); // \r\n--<b>--\r\n

		std::string::size_type emitUpto = 0;
		if (m_buf.size() > overlap)
			emitUpto = m_buf.size() - (overlap - 1);

		if (emitUpto > 0)
		{
			if (emitDataChunk(emitUpto)) { progress = true; return D_OK; }
		}
		// possible split boundary at tail → need more bytes
		return D_MORE;
	}

	// 2) Boundary candidate at index k (k points to the CRLF before "--<b>")
	if (k > 0)
	{
		if (emitDataChunk(k)) progress = true; // emit payload BEFORE the CRLF
	}

	// Now buffer begins with the boundary line: "\r\n--<b>" or "\r\n--<b>--"
	const std::string::size_type need =
		isClosing ? (2 + m_delimClose.size() + 2)   // \r\n + --<b>-- + \r\n
				: (2 + m_delim.size()     + 2);  // \r\n + --<b>   + \r\n

	if (m_buf.size() < need)
	{
		// Partial boundary line split across feeds → wait
		return D_MORE;
	}

	// 3) Validate the trailing CRLF at the end of the boundary line
	const std::string::size_type head =
		isClosing ? (2 + m_delimClose.size())
				: (2 + m_delim.size());

	if (m_buf.compare(head, 2, "\r\n") != 0)
		return D_ERR; // malformed line

	// 4) End current part, consume the boundary line, and transition
	if (m_onEnd) m_onEnd(m_user);

	m_buf.erase(0, need);

	if (isClosing)
	{
		m_st = S_DONE;
		return D_DONE;
	}
	else
	{
		m_st = S_HEADERS;
		progress = true;
		return D_OK;
	}
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
				bool did = false;
				DRes r = s_dataFlow(did);
				if (r == D_ERR)
				{
					enterError();
					return ERR;
				}
				if (r == D_DONE)
					return DONE;
				if (r == D_OK)
					progress = true;
				if (r == D_MORE)
					break;
				break;
			}
			case S_DONE :
				return DONE;
				break;
			case S_ERROR :
				enterError();
				return ERR;
				break;
			default :
				break;
		}
	}
	return MORE;
}

int MultipartStreamParser::mp_state() const
{
        return static_cast<int>(m_st);
}

bool MultipartStreamParser::isDone() const
{
        return m_st == S_DONE;
}
