#ifndef FILE_UTILS_HPP
#define FILE_UTILS_HPP
#include <string>
 namespace file_utils {
       // Does this path exist?
       bool exists(const std::string& path);
    
       // Read the entire file into a string
       std::string readFile(const std::string& path);
    
       // Simplify “/foo/../bar” etc. into a canonical path
       std::string normalize(const std::string& rawPath);
    
       // Guess the MIME type (e.g. ".html" → "text/html")
       std::string getMimeType(const std::string& path);
     }

#endif
