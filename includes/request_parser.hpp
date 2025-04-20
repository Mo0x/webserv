#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

#include "request_reponse_struct.hpp"
#include <string>

Request parseRequest(const std::string &raw);

#endif
