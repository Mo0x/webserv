#include <iostream>

#include "Chunked.hpp"
#include "utils.hpp"

ChunkedDecoder::ChunkedDecoder() :
	m_state(S_SIZE), 
	m_total(0),
	m_curr_size(0),
	m_status_code (0)
{
	m_data.empty();
	m_error.empty();
	m_line.empty();
}

bool ChunkedDecoder::done() const
{
	return (m_state == S_DONE);
}

bool ChunkedDecoder::error() const
{
	return (m_state == S_ERROR);
}

int ChunkedDecoder::getStatusCode() const
{
	return (m_status_code);
}

const std::string &ChunkedDecoder::getBody() const
{
	return (m_data);
}

void ChunkedDecoder::handleSizeState(const char *buf, size_t len, size_t &i)
{
	// append buf[i] into m_line until CRLF or we run out;
	// if CRLF reached, parse hex, set m_curr_size, pick next state
	while (i < len)
	{
		// accumulate byte
		m_line.push_back(buf[i]);
		++i;

		// arbitrary DDOS guard
		if (m_line.size() > 8192)
		{
			m_state = S_ERROR;
			m_status_code = 400;
			m_line.clear(); // optional tidy
			break; // break out of this helper's while
		}
		// did we collect a full line ending in "\r\n" ?
		if (m_line.size() > 1 &&
			m_line[m_line.size() - 2] == '\r' &&
			m_line[m_line.size() - 1] == '\n')
		{
			// strip \r\n
			std::string no_crlf_string = m_line.substr(0, m_line.size() - 2);

			// ignore ";extension"
			std::string::size_type semi = no_crlf_string.find(';', 0);
			std::string hex_part = (semi == std::string::npos)
				? no_crlf_string
				: no_crlf_string.substr(0, semi);

			size_t parsed_hex;
			if (!std_to_hex(hex_part, parsed_hex))
			{
				m_state = S_ERROR;
				m_status_code = 400;
				m_line.clear();
				break;
			}
			if (parsed_hex == 0)
			{
				// last-chunk case
				m_curr_size = 0;
				m_state = S_TRAILERS;
				m_line.clear();
				break;
			}
			else
			{
				// normal chunk case
				m_curr_size = parsed_hex;
				m_state = S_DATA;
				m_line.clear();
				break;
			}
		}
		// else: we haven't hit CRLF yet, keep looping.
		// if we leave the while because i == len, we just return with
		// m_state still == S_SIZE and a partially-filled m_line.
	}

}

void ChunkedDecoder::handleDataState(const char *buf, size_t len, size_t &i, size_t max_body_size)
{
	//Basically take m_current_size bytes and push into m_data, then either m_curr_size == 0
	// and we need to go into S_CRLF or m_curr_size > 0 and it means we still have data handing
	size_t need = m_curr_size; // how many bytes we still need to finish this chunk
	size_t avail = len - i; // how many bytes are currently available in this recv buffer
	size_t take = (avail < need) ? avail : need; // we’ll only consume this many bytes right now
	//here we enforce body_size, before appending, prevent us from writing in data if we know its gonna be too big
	//append those bytes to the decoded body buffer
	if (take > 0)
	{
		if (m_total > max_body_size)
		{
			m_state = S_ERROR;
			m_status_code = 413; // Payload too large
			return ;
		}
		m_data.append(buf + i, take);
		m_curr_size -= take;
		m_total += take;
		i += take;
	}

	// Only IF m_curr_size == 0 we switch to S_CRLF, otherwise it means we are still waiting data from
	// next recv and we need to stay in S_DATA
	if (m_curr_size == 0)
	{
		m_state = S_CRLF;
	}
}

void ChunkedDecoder::handleCrlfState(const char *buf, size_t len, size_t &i)
{
	//its the big http formating checker, now the next two bytes must be \r\n
	//the trick is it can come from two recv call instead of one, and thats valid
	// if the two next bytes arent exactly \r\n we throw ---> 400
	while (i < len && m_state == S_CRLF)
	{
		m_line.push_back(buf[i]);
		i++;
		if (m_line.size() > 2)
		{
			// that impossible we want \r\n exactly
			m_state = S_ERROR;
			m_status_code = 400;
			m_line.clear();
			break ;
		}
		if (m_line.size() == 1)
		{
			if (m_line[0] != '\r')
			{
				m_state = S_ERROR;
				m_status_code = 400;
				m_line.clear();
				break ;
			}
			continue ;
		}
		if (m_line.size() == 2)
		{
			if (m_line[1] != '\n')
			{
				m_state = S_ERROR;
				m_status_code = 400;
				m_line.clear();
				break ;
			}
			//if those conditions upper didnt trigger we good we have \r\n at the end of the
			// chunk of data, we go back to S_SIZE state
			m_line.clear();
			m_state = S_SIZE;
			break ;
		}
	}
}

void ChunkedDecoder::handleTrailersState(const char *buf, size_t len, size_t &i)
{
	while (i < len && m_state == S_TRAILERS)
	{
		m_line.push_back(buf[i]);
		i++;
		// Nobody likes you DOS wannabe
		if (m_line.size() > 8192)
		{
			m_state = S_ERROR;
			m_status_code = 400;
			m_line.clear();
			break;
		}
		if (m_line.size() > 1
			&& m_line[m_line.size() - 2] == '\r'
			&& m_line[m_line.size() - 1] == '\n')
		{
			std::string line_no_crlf = m_line.substr(0, m_line.size() - 2);
			m_line.clear();

			//if line_no_crlf is empty, its the end of the chunk ! we done
                        if (line_no_crlf.size() == 0)
                        {
                                m_state = S_DONE;
                                std::cerr << "[chunked] reached S_DONE" << std::endl;
                                break ;
                        }
			// else: it's just a trailer header (e.g. "Checksum: deadbeef")
			// ignore it and keep looping to read more trailers
		}
	}
}

void ChunkedDecoder::drainTo(std::string &dst)
{
	if (!m_data.empty())
	{
		dst.append(m_data);
		m_data.clear();
	}
}

bool ChunkedDecoder::hasError() const
{
	return m_state == S_ERROR; // uses your private enum/state
}

size_t ChunkedDecoder::feed(const char *buf, size_t len, size_t max_body_size)
{
	size_t i = 0; //what we will return and our incrmentor in buf
	while (i < len && m_state != S_DONE && m_state != S_ERROR)
	{
		switch (m_state)
		{
			case S_SIZE :
				handleSizeState(buf, len, i);
				break;
			case S_DATA :
				handleDataState(buf, len, i, max_body_size);
				break;
			case S_CRLF :
				handleCrlfState(buf, len, i);
				break;
			case S_TRAILERS :
				handleTrailersState(buf, len, i);
				break;
			default :
				break;
		}
	}
	return i;
}
