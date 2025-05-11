/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abedin <abedin@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 15:48:28 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/05 09:33:12 by abedin           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include "webserv.hpp"
# define READPIPE_BUFFER_SIZE 1024UL
# define WRITEPIPE_BUFFER_SIZE 1024UL
# define FILEDOWNLOAD_MAXSIZE 10000000UL // 10 MB
# define FILEUPLOAD_MAXSIZE 10000000UL // 10 MB
# define READFILE_BUFFER_SIZE 1024UL
# define WRITEFILE_BUFFER_SIZE 1024UL

class Response
{
	public:
		Response(Request* request, t_portaddr clientPortaddr, 
			std::vector<t_vserver>& vservers, std::set<int>& vservIndexes);
		Response(int errcode);
		~Response();
		int				produceResponse();
		void			logResponseBuffer();
		char*			getResponseBuffer();
		unsigned long	getResponseSize();

	private:
		// Memory of server's config
		Request*			_request;
		t_portaddr			_clientPortaddr;
		// Identification of virtual server and location to which request is directed
		t_vserver*			_vserv;
		t_location*			_location;
		// Internal state
		char*				_responseBuffer;
		unsigned long		_responseSize;
		std::vector<char>	_cgiOutput;
		std::vector<char>	_cgiOutputBody;
		// Identify virtual server targeted by request
		t_vserver*			identifyVserver(std::vector<t_vserver>& vservers, std::set<int>& vservIndexes);
		t_location*			identifyLocation();
		// Find path to resource
		int					pathToResource(std::string& fullPath,
								bool checkFile, bool checkExec, size_t* fileSize, bool isUpload);
		int					buildFullPath(std::string& fullPath, bool isUpload);
		int					checkResolvedFullPath(std::string& fullPath, bool isUpload);
		bool				isCGIRequest();
		// Generate CGI response
		int					generateCGIResponseClientRedirectDoc(std::map<std::string, std::string>& cgiHeaders,
								std::string& statusCode, std::string& statusDesc);
		int					generateCGIResponseClientRedirect(std::map<std::string, std::string>& cgiHeaders,
								std::string& statusCode, std::string& statusDesc);
		int					generateCGIResponseLocalRedirect(std::map<std::string, std::string>& cgiHeaders,
								std::string& statusCode, std::string& statusDesc);
		int					generateCGIResponseDoc(std::map<std::string, std::string>& cgiHeaders,
								std::string& statusCode, std::string& statusDesc);
		// Process CGI output
		int					parseCGIStatusLine(std::string& line, std::string& statusCode, std::string& statusDesc);
		int					parseCGIHeaderLine(std::string& line, std::map<std::string, std::string>& cgiHeaders);
		int					parseCGIHeaders(std::map<std::string, std::string>& cgiHeaders,
								std::string& statusCode, std::string& statusDesc);
		unsigned long		findCGIOutputHeader();
		int					processCGIOutput();
		// Fork and execute CGI script
		void				setupCGIEnv(std::string& fullPath, std::map<std::string, std::string>& env);
		char**				createEnvArray(std::string& fullPath, int* envArraySize);
		void				childExecuteCGI(std::string& fullPath, int fdInput, int fdOutput);
		int					feedBodyToChild(int fdOutput);
		int					readResponseFromChild(int fdInput);
		int					waitForChildCGI(pid_t pid, int* pipeRequest, int* pipeResponse);
		int					forkCGI(std::string& fullPath);
		int					handleCGI();
		// Handle Non-CGIs
		void				makeSuccessResponse(int infocode);
		void				makeErrorResponse(int errcode);
		int					readFullFileInBuffer(std::string& fullPath, size_t fileSize, char* buffer);
		int					makeFileResponse(std::string& fullPath, size_t fileSize);
		std::string			getResponseBodyType();
		int					handleGet();
		int					writeFullBufferInFile(std::string& fullPath, size_t bufferSize, char *buffer);
		int					handlePost();
		int					handleDelete();

};

#endif
