/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abedin <abedin@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 15:48:28 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/16 13:43:56 by abedin           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#ifndef REQUEST_HPP
# define REQUEST_HPP

# include "webserv.hpp"

// <Request> is a parsing class without any "server logic" :
// 		1. as client reads bytes from request headers, static method <Request.containsHeadersEnd>
// 		  checks if headers separator "\r\n\r\n" is present in those bytes
// 		2. once <Client> instance received all request headers from Client
// 		  and creates an instance of <Request>,
// 			method <Request.parse> stores headers in a <std::map>
//			and checks compliance of the HTTP request to the http/tcp standard
// 				(returns 1 if the request is not compliant)
// 		3. once headers are parsed, method <Request.identifyTargeted> identifies
// 		   the virtual server and its location targeted by the request (vserv mandatory, location optional)
// 		4. as <Client> reads bytes belonging to request's body,
// 			method <allocateBody> allocates a char* buffer for them
// 			and method <appendToBody> stores them in the buffer (unchunking them if necessary)

class Request
{
	public:
		Request(t_mainSocket& _mainSocket);
		~Request();
		bool				parse(const std::string& raw_request);
		void				identifyTargeted(std::vector<t_vserver>& vservers, std::set<int>& vservIndexes);
		void				allocateBody();
		int					appendToBody(char* buffer, size_t bufferSize);
		int					redirectPath(std::string newPath, std::string newMethod);
		void				logRequest();
		// Early checks on request acceptability
		int					bodySizeForbidden();
		int					methodForbidden();
		// Static method to check completeness of request headers
		static ssize_t		containsHeadersEnd(std::string& previousBytes, char* buffer, size_t bufferSize);
		// Memory of server's config
		t_mainSocket&						_mainSocket;
		// Virtual server and location responsible for request
		t_vserver*							_vserv;
		t_location*							_location;
		// Storing request body
		bool								_hasBody;
		bool								_chunkedBody;
		unsigned long						_bodySize;
		unsigned long						_nReceivedBodyBytes;
		char*								_body;
		// State of chunked body reception
		size_t								_chunksTotalSize;
		size_t								_chunkSize;
		size_t								_nReceivedChunkBytes;
		std::string							_prevChunkHead;
		int									_chunkSepOffset;
		int									_lastChunkDone;
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
		// Parsing headers
		bool				parseFirstLine(const std::string& rawHeaders);
		bool				parseHeaders(const std::string& rawHeaders);
		bool				checkHeaders();
		void				extractFromURL();
		void				extractFromHost();
		// Handling chunked body
		void				initChunkedBody();
		void				adaptBodySizeToChunks();
		ssize_t				searchForChunkHead(char *buffer, size_t bufferSize);
		int					processChunkHead(char *buffer, size_t bufferSize);
		int					appendToChunkedBody(char* buffer, size_t bufferSize);
		// Identifying virtual server and location responsible for request
		t_vserver*			identifyVserver(std::vector<t_vserver>& vservers, std::set<int>& vservIndexes);
		t_location*			identifyLocation();

};

#endif
