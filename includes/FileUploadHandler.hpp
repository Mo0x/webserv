#ifndef FILEUPLOADHANDLER_HPP
#define FILEUPLOADHANDLER_HPP

#include <string>
#include <cstdio>   // FILE*

class FileUploadHandler
{
public:
        FileUploadHandler() : m_fullPath(), m_fp(0) {}

	// Open a file in uploadDir using safeFilename (already sanitized).
	// Returns true on success.
	bool open(const std::string& uploadDir, const std::string& safeFilename);

	// Write up to n bytes. Returns bytes actually written (may be short).
	size_t write(const char* buf, size_t n);

	// Close if open.
	void close();

	// Full path of the opened file (empty if not open).
	const std::string& path() const { return m_fullPath; }

	bool isOpen() const { return m_fp != 0; }

private:
	std::string m_fullPath;
	FILE*       m_fp;
};

#endif
