/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilevy <ilevy@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 15:48:28 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/15 07:50:04 by ilevy            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#ifndef REQUEST_HPP
# define REQUEST_HPP

# include "webserv.hpp"

// Request takes in the request from Client when static method 'requestIsComplete()'
// has verified that it is complete (ie. has a double linebread and a body of the right size)
// Request buffer should be given to 'parse_request()' method
// Parse will check the compliance of the HTTP request to the http/tcp standard
// and return false if the request is not compliant

# define MAX_CHUNKED_SIZE 1024 * 1024

class Request
{
	public:
		Request(t_mainSocket& _mainSocket, int socket_fd );
		~Request();
		bool				parse(const std::string& raw_request);
		void				allocateBody();
		void				appendToBody(char* buffer, size_t bufferSize);
		int					redirectPath(std::string newPath);
		void				logRequest();
		// Static method to check completeness of request headers
		static ssize_t		containsHeadersEnd(char *lastBuffer, char* buffer, size_t bufferSize);
		// Memory of server's config
		t_mainSocket&						_mainSocket;
		int									_socketFd;
		// Storing request body
		bool								_hasBody;
		bool								_hasChunked;
		unsigned long						_bodySize;
		unsigned long						_nReceivedBodyBytes;
		char*								_body;
		// Results of general parsing
		std::string							_method;
		std::string							_path;
		std::string							_httpVersion;
		std::map<std::string, std::string>	_headers;
		bool								_parsed;
		// Results of specific parsing
		std::string							_filePath; // the path, without path info and query string
		std::string							_pathInfo;
		std::string							_queryString;
		std::string							_extension;
		bool								_toDir;
		t_portaddr							_hostPortaddr;
		std::string							_hostName;

	private:
		bool				parseFirstLine(const std::string& rawHeaders);
		bool				parseHeaders(const std::string& rawHeaders);
		bool				parseChunkedBody( int socket_fd );
		std::string			readLine( int socket_fd );
		bool				checkHeaders();
		void				extractFromURL();
		void				extractFromHost();
		
};

#endif
