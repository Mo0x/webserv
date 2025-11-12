#ifndef MULTIPART_STREAM_PARSER_HPP
#define MULTIPART_STREAM_PARSER_HPP

#include <string>
#include <map>

class MultipartStreamParser
{
	public:
	typedef void (*PartBeginCb)(void* user, const std::map<std::string,std::string>& headers);
	typedef void (*PartDataCb)(void* user, const char* buf, size_t n);
	typedef void (*PartEndCb)(void* user);

	enum Result {	MORE,
					DONE,
					ERR };

	
	

	MultipartStreamParser();

        void reset(const std::string& boundary,
                        PartBeginCb onBegin,
                        PartDataCb  onData,
                        PartEndCb   onEnd,
                        void* user);

        Result feed(const char* data, size_t n);  // body bytes only (unchunked)

        int mp_state() const;
        bool isDone() const;

        private:
	// Minimal Finite State Machine + rolling buffer
	enum S {
			S_PREAMBLE,
			S_HEADERS,
			S_DATA,
			S_DONE,
			S_ERROR };

	enum HRes{	H_MORE,
				H_OK,
				H_ERR };
	enum DRes{	D_MORE,
				D_OK,
				D_DONE,
				D_ERR };

	S                  m_st;
	std::string        m_boundary;   // e.g. "----WebKit..."
	std::string        m_delim;      // "--" + boundary
	std::string        m_delimClose; // "--" + boundary + "--"
	std::string        m_buf;        // rolling window for scans
	std::map<std::string,std::string> m_curHeaders;

	PartBeginCb        m_onBegin;
	PartDataCb         m_onData;
	PartEndCb          m_onEnd;
	void*              m_user;

	// S_PREAMBLE
	bool consumeToFirstBoundary();
	// S_HEADERS
	HRes parsePartHeaders(std::string::size_type &outConsumed); // read headers up to CRLFCRLF into m_curHeaders
	// S_DATA
	bool emitDataChunk(size_t upto);
	bool findNextBoundary(std::string::size_type &k, bool &isClosing);
	DRes s_dataFlow(bool &progress);

	//S_ERROR
	void enterError();
};

#endif
