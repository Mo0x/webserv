/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   MultipartParser.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: wkeiser <wkeiser@student.42mulhouse.fr>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 21:04:55 by wkeiser           #+#    #+#             */
/*   Updated: 2025/04/13 21:04:57 by wkeiser          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "MultipartParser.hpp"
#include <sstream>
#include <stdexcept>

MultipartParser::MultipartParser(const std::string &body, const std::string &contentType)
    : _body(body), _contentType(contentType)
{}

MultipartParser::~MultipartParser()
{}

// Extracts the boundary by searching for "boundary=" in the content type header.
std::string MultipartParser::extractBoundary(const std::string &contentType) const
{
    std::string boundaryPrefix = "boundary=";
    size_t pos = contentType.find(boundaryPrefix);
    if (pos == std::string::npos)
    {
        throw std::runtime_error("Boundary not found in content type");
    }
    pos += boundaryPrefix.length();
    // The boundary delimiter used in the multipart body is preceded by two dashes.
    return "--" + contentType.substr(pos);
}

// Parses the multipart body into its parts.
std::vector<MultipartPart> MultipartParser::parse()
{
    std::vector<MultipartPart> parts;
    std::string boundary = extractBoundary(_contentType);
    
    size_t pos = 0;
    while (true)
    {
        size_t start = _body.find(boundary, pos);
        if (start == std::string::npos)
            break;
        start += boundary.length();
        // If the boundary ends with "--", it's the closing boundary.
        if (_body.substr(start, 2) == "--")
            break;
        // Skip CRLF following the boundary.
        if (_body.substr(start, 2) == "\r\n")
            start += 2;
            
        // Find the next occurrence of the boundary.
        size_t end = _body.find(boundary, start);
        if (end == std::string::npos)
            break;
        std::string partData = _body.substr(start, end - start);
        
        // Split the part into headers and body (look for double CRLF).
        size_t headerEnd = partData.find("\r\n\r\n");
        if (headerEnd == std::string::npos)
        {
            pos = end;
            continue; // No header separator found, skip this part.
        }
        std::string headerBlock = partData.substr(0, headerEnd);
        std::string content = partData.substr(headerEnd + 4); // skip over "\r\n\r\n"
        
        MultipartPart part;
        part.content = content;
        part.isFile = false;
        part.filename = "";
        
        // Process each header line.
        std::istringstream headerStream(headerBlock);
        std::string line;
        while (std::getline(headerStream, line)) {
            if (!line.empty() && line[line.size()-1] == '\r')
            {
                line.erase(line.size()-1); // Remove trailing carriage return.
            }
            size_t sep = line.find(":");
            if (sep != std::string::npos)
            {
                std::string key = line.substr(0, sep);
                std::string value = line.substr(sep + 1);
                // Trim any leading/trailing spaces.
                size_t first = value.find_first_not_of(" ");
                size_t last = value.find_last_not_of(" ");
                if (first != std::string::npos && last != std::string::npos)
                    value = value.substr(first, last - first + 1);
                part.headers[key] = value;
                
                // If Content-Disposition contains "filename=", mark part as a file upload.
                if (key == "Content-Disposition") {
                    size_t filenamePos = value.find("filename=");
                    if (filenamePos != std::string::npos) {
                        size_t startQuote = value.find("\"", filenamePos);
                        size_t endQuote = std::string::npos;
                        if (startQuote != std::string::npos)
                        {
                            endQuote = value.find("\"", startQuote + 1);
                        }
                        if (startQuote != std::string::npos && endQuote != std::string::npos)
                        {
                            part.filename = value.substr(startQuote + 1, endQuote - startQuote - 1);
                            part.isFile = true;
                        }
                    }
                }
            }
        }
        parts.push_back(part);
        pos = end;
    }
    return parts;
}
