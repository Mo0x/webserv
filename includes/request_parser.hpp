#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

#include <string>

#include "request_reponse_struct.hpp"

Request parseRequest(const std::string &raw);

#endif
