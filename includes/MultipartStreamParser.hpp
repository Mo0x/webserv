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

    enum Result { MORE, DONE, ERR };

    MultipartStreamParser();

    void reset(const std::string& boundary,
               PartBeginCb onBegin,
               PartDataCb  onData,
               PartEndCb   onEnd,
               void* user);

    Result feed(const char* data, size_t n);  // body bytes only (unchunked)

private:
    // Minimal FSM + rolling buffer
    enum S { S_PREAMBLE, S_HEADERS, S_DATA, S_DONE, S_ERROR };
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

    bool consumeToFirstBoundary();
    bool parsePartHeaders(); // read headers up to CRLFCRLF into m_curHeaders
    bool emitDataChunk(size_t upto);
    void enterError();
};

#endif
