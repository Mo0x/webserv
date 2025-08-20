#ifndef FILEUPLOADHANDLER_HPP
#define FILEUPLOADHANDLER_HPP

#include <string>
#include "MultipartParser.hpp"

class FileUploadHandler
{
public:
    // Saves the uploaded file part to the specified directory.
    // Optionally enforces a maximum file size (0 means no limit).
    // Returns the full path where the file was saved.
    static std::string saveUploadedFile(const MultipartPart &part,
                                          const std::string &uploadPath,
                                          size_t maxFileSize = 0);
};

#endif
