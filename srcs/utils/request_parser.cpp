/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   request_parser.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/20 19:21:04 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/20 19:39:40 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "request_reponse_struct.hpp"
#include <sstream>
#include <iostream>

Request parseRequest(const std::string &raw)
{
	Request	req;
	std::istringstream stream(raw);
	std::string line;

	if (std::getline(stream, line))
	{
		std::istringstream lineStream(line);
		lineStream >>req.method >> req.path >> req.http_version;
	}
	while (std::getline(stream, line) && line != "\r")
	{
		size_t colon = line.find(':');
		if (colon != std::string::npos)
		{
			std::string key = line.substr(0, colon);
			std::string value = line.substr(colon + 1);
			while (!value.empty() && (value[0] == ' '|| value[0] == '\t'))
				value.erase(0, 1);
			if (!value.empty() && value[value.size() - 1] == '\r')
				value. erase(value.size() - 1);
			req.headers[key] = value;
		}
	}

	size_t body_pos = raw.find("\r\n\r\n");
	if (body_pos != std::string::npos && body_pos + 4 < raw.length())
		req.body = raw.substr(body_pos + 4);
	return req;
}