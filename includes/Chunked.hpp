#ifndef CHUNKED_HPP
#define CHUNKED_HPP
#include <string>

class ChunkedDecoder
{
	private:
	enum State
	{
		S_SIZE,      // reading "<hex>[;extensions]\r\n" into m_line
		S_DATA,      // reading exactly m_curr_size bytes of chunk data
		S_CRLF,      // expecting the "\r\n" that terminates that chunk
		S_TRAILERS,  // after size 0: read trailer lines until blank line "\r\n\r\n"
		S_DONE,      // finished
		S_ERROR      // bad request / too large
	};
	State		m_state; //our current state while reading
	size_t		m_total; //total we decoded so far
	size_t		m_curr_size; // remaining bytes for current chunk 
	std::string	m_data; // decoded body
	std::string	m_error; // 400/413
	std::string	m_line; // accumulate size line of trailer line until CRLF
	int			m_status_code;

	public:
	ChunkedDecoder();

	// Main function, feed update m_state and return how many bytes were consumed from buf
	size_t feed(const char *buf, size_t len, size_t max_body_size);
	bool done() const; //simple check to s_state
	bool error() const;
	int getStatusCode() const;
	const std::string &getBody() const;
	//helpers for feed()
	void handleSizeState(const char *buf, size_t len, size_t &i);
	void handleDataState(const char *buf, size_t len, size_t &i, size_t max_body_size);
	void handleCrlfState(const char *buf, size_t len, size_t &i);
	void handleTrailersState(const char *buf, size_t len, size_t &i);
	void drainTo(std::string &dst);  // append internal decoded data to dst and clear it
	bool hasError() const;           // true if a fatal parse error occurred

};

#endif