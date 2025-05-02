/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: administyrateur <administyrateur@studen    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 16:08:40 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/02 16:47:30 by administyra      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../hpp_files/webserv.hpp"

Request::Request( void )
{
	if (LOGSV == 1)
		std::cout << "[LOGSV]/ Request constructor called." << std::endl;
}

Request::~Request( void )
{
	if (LOGSV == 1)
		std::cout << "[LOGSV]/ Request destructor called." << std::endl;
}

const std::string& Request::getBody( void ) const
{
	return (this->_body);
}

const std::map<std::string, std::string>& Request::getHeaders( void ) const
{
	return (this->_headers);
}

const std::string& Request::getPath( void ) const
{
	return (this->_path);
}

const std::string& Request::getHttpVersion( void ) const
{
	return (this->_httpVersion);
}

const std::string& Request::getMethod( void ) const
{
	return (this->_method);
}

bool	Request::isComplete( const std::string& buffer ) const
{
	size_t	header_end;
	size_t	content_length;

	header_end = buffer.find("\r\n\r\n");
	if (header_end == std::string::npos)
		return (false);
	content_length = buffer.find("Content-Length:");
	//This detects the POST method, which contains a body
	if (content_length != std::string::npos)
	{
		size_t		value_start;
		size_t		value_end;
		std::string	length_string;
		int			length;

		// This will extract the numeric value of Content-length
		value_start = buffer.find_first_not_of(" ", content_length + 15);
		value_end = buffer.find("\r\n", value_start);
		length_string = buffer.substr(value_start, value_end - value_start);
		length = std::atoi(length_string.c_str());

		//If we have received enough data (the whole body), then this is true.
		if (buffer.size() >= header_end + 4 + length)
			return (true);
		return (false);
	}
	//For GET and DELETE methods
	return (true);
}

bool	Request::parse( const std::string& raw_request )
{
	size_t	header_end;

	header_end = raw_request.find("\r\n\r\n");
	if (header_end == std::string::npos)
	{
		std::cout << "111111" << std::endl;
		return (false);
	}
	if (this->parse_request(raw_request, header_end) == false)
	{
		std::cout << "222222" << std::endl;
		return (false);
	}
	if (this->parse_headers(raw_request, header_end) == false)
	{
		std::cout << "333333333" << std::endl;
		return (false);
	}
	if (this->_headers.count("Content-Length"))
	{
		int contentLength = std::atoi(this->_headers["Content-Length"].c_str());
		std::string body_section = raw_request.substr(header_end + 4);
		if ((int)body_section.size() < contentLength)
		{
			std::cout << "444444444" << std::endl;
			return (false);
		}
		this->_body = body_section.substr(0, contentLength);
	}
	this->_parsed = true;
	return (true);
}

bool	Request::parse_request( const std::string& raw_request, size_t header_end )
{
	size_t	line_end;
	size_t	method_end;
	size_t	path_end;
	std::string	header_string;
	std::string	body_string;
	std::string request_line;

	header_string = raw_request.substr(0, header_end);
	body_string = raw_request.substr(header_end + 4);
	line_end = header_string.find("\r\n");
	if (line_end == std::string::npos)
		return (false);
	request_line = header_string.substr(0,line_end);

	method_end = request_line.find(' ');
	if (method_end == std::string::npos)
		return (false);
	this->_method = request_line.substr(0, method_end);

	path_end = request_line.find(' ', method_end + 1);
	if (path_end == std::string::npos)
		return (false);

	this->_path = request_line.substr(method_end + 1, path_end - method_end - 1);
	this->_httpVersion = request_line.substr(path_end + 1);
	return (true);
}

bool	Request::parse_headers( const std::string& raw_request, size_t header_end )
{
	size_t		line_start;
	size_t		line_end;
	size_t		colon_pos;
	std::string	header_string;
	std::string	line_content;

	header_string = raw_request.substr(0, header_end);
	line_start = header_string.find("\r\n") + 2;
	while (line_start < header_end)
	{
		line_end = header_string.find("\r\n", line_start);
		if (line_end == std::string::npos)
			break;
		line_content = header_string.substr(line_start, line_end - line_start);
		colon_pos = line_content.find(':');
		if (colon_pos != std::string::npos)
		{
			std::string key = line_content.substr(0, colon_pos);
			std::string value = line_content.substr(colon_pos + 1);
			key.erase(key.find_last_not_of(" ") + 1);
			value.erase(0, value.find_first_not_of(" "));
			value.erase(value.find_last_not_of(" ") + 1);
			this->_headers[key] = value;
		}
		line_start = line_end + 2;
	}
	return (true);
}

void	Request::log_request(void)
{
	std::cout << "===== REQUEST COMPONENTS\n";
	std::cout << "Method: " << this->getMethod() << "\n";
	std::cout << "Path: " << this->getPath() << "\n";
	std::cout << "Headers:\n";
	for (auto it = this->getHeaders().begin(); it != this->getHeaders().end(); ++it)
		std::cout << it->first << ": " << it->second << "\n";
	std::cout << "=====\n";
}

