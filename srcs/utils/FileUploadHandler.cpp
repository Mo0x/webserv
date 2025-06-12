/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileUploadHandler.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: wkeiser <wkeiser@student.42mulhouse.fr>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 21:08:42 by wkeiser           #+#    #+#             */
/*   Updated: 2025/04/13 21:08:43 by wkeiser          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "FileUploadHandler.hpp"
#include <fstream>
#include <stdexcept>

std::string FileUploadHandler::saveUploadedFile(const MultipartPart &part,
                                                  const std::string &uploadPath,
                                                  size_t maxFileSize)
  {
    if (!part.isFile)
    {
        throw std::runtime_error("Part is not a file upload");
    }
    if (maxFileSize > 0 && part.content.size() > maxFileSize)
    {
        throw std::runtime_error("Uploaded file exceeds maximum allowed size");
    }
    // Build the complete file path; add a '/' if necessary.
    std::string filePath = uploadPath;
    if (uploadPath[uploadPath.size() - 1] != '/')
        filePath += "/";
    filePath += part.filename;
    
    std::ofstream ofs(filePath.c_str(), std::ios::binary);
    if (!ofs)
    {
        throw std::runtime_error("Failed to open file for writing: " + filePath);
    }
    ofs.write(part.content.c_str(), part.content.size());
    ofs.close();
    return filePath;
}
