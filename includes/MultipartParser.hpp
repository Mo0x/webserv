/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   MultipartParser.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: wkeiser <wkeiser@student.42mulhouse.fr>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 21:04:40 by wkeiser           #+#    #+#             */
/*   Updated: 2025/04/13 21:04:41 by wkeiser          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MULTIPARTPARSER_HPP
#define MULTIPARTPARSER_HPP

#include <string>
#include <vector>
#include <map>

// Structure for storing a parsed part (form field or file upload)
struct MultipartPart
{
    std::map<std::string, std::string> headers;
    std::string content;
    bool isFile;         // true if this part represents a file upload
    std::string filename; // filename if provided in headers
};

class MultipartParser
{
public:
    // Constructor takes the full request body and the Content-Type header.
    MultipartParser(const std::string &body, const std::string &contentType);
    ~MultipartParser();

    // Parse returns a vector of MultipartPart entries.
    std::vector<MultipartPart> parse();

private:
    // Helper to extract the boundary string from the content type.
    std::string extractBoundary(const std::string &contentType) const;

    std::string _body;
    std::string _contentType;
};

#endif
