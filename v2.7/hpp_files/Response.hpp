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
# define READPIPE_BUFFER_SIZE 65536UL
# define WRITEPIPE_BUFFER_SIZE 65536UL
# define READFILE_BUFFER_SIZE 65536UL
# define WRITEFILE_BUFFER_SIZE 65536UL
# define FILEDOWNLOAD_MAXSIZE 10000000UL // 10 MB
# define FILEUPLOAD_MAXSIZE 100000000UL // 10 MB

// <Response> is the main class responsible for "server logic", ie. serving resources to client :
// 		\ it gets (a pointer to) the <Request> instance created by <Client>,
// 			and registers the virtual server and location that must handle it
// 		\ it creates with method <produceResponse> the response to that request,
// 			accepting GET / POST / DELETE / (PUT)
// 		\ it handles CGI requests using fork and pipes

class Response
{
	public:
		Response(Request* request, t_portaddr clientPortaddr);
		Response(std::string status);
		~Response();
		int				checkCGIFinished();
		int				produceResponse();
		void			logResponseBuffer();
		char*			getResponseBuffer();
		unsigned long	getResponseSize();
		int				getCGIChildFd();

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
		// Buffers for CGI output
		std::vector<char>	_cgiOutput;
		std::vector<char>	_cgiOutputBody;
		// Memory for communication with CGI child
		pid_t				_cgiPid;
		int					_cgiReadFd;
		// Find path to resource
		int					buildFullPathUpload(std::string& dirPath);
		int					pathToUpload(std::string& fullPath, bool isDir);
		int					buildFullPathFile(std::string& fullPath, std::string& origPath);
		int					checkResolvedDirPath(std::string& dirPath);
		int					pathToFile(std::string& fullPath,
								bool checkFile, bool checkExec, size_t* fileSize);
		// Generate CGI response
		int 				checkCGIOutputBody(std::multimap<std::string, std::string>& cgiHeaders);
		int					generateCGIResponseClientRedirect(std::multimap<std::string, std::string>& cgiHeaders,
								std::string& statusCode, std::string& statusDesc);
		int					generateCGIResponseLocalRedirect(std::multimap<std::string, std::string>& cgiHeaders,
								std::string& statusCode, std::string& statusDesc);
		int					generateCGIResponseWithDoc(std::multimap<std::string, std::string>& cgiHeaders,
								std::string& statusCode, std::string& statusDesc);
		int					generateCGIResponseClientRedirectDoc(std::multimap<std::string, std::string>& cgiHeaders,
								std::string& statusCode, std::string& statusDesc);
		int					generateCGIResponseNoredir(std::multimap<std::string, std::string>& cgiHeaders,
								std::string& statusCode, std::string& statusDesc);
		// Process CGI output
		int					parseCGIStatusLine(std::string& line, std::string& statusCode, std::string& statusDesc);
		int					parseCGIHeaderLine(std::string& line, std::multimap<std::string, std::string>& cgiHeaders);
		int					parseCGIHeaders(std::multimap<std::string, std::string>& cgiHeaders,
								std::string& statusCode, std::string& statusDesc);
		int					findCGIOutputHeader(size_t& ind);
		int					processCGIOutput();
		// Fork and execute CGI script
		void				setupCGIEnv(std::string& fullPath, std::map<std::string, std::string>& env);
		char**				createEnvArray(std::string& fullPath, int* envArraySize);
		void				childExecuteCGI(std::string& fullPath, std::string& interpreterPath, int fdInput, int fdOutput);
		int					feedBodyToChild(int fdOutput);
		int					handleCGIFinished();
		int					startWaitCGI(pid_t pid, int bodyWriteFd, int cgiReadFd);
		int					forkCGI(std::string& fullPath, std::string& interpreterPath);
		int					handleCGI();
		// Generic responses
		void				makeResponseFromString(std::string& responseStr);
		int					makeRedirResponse(std::string status, std::string newPath);
		int					makeSuccessResponse(std::string status);
		int					makeErrorResponseDefault(std::string status);
		int					makeErrorResponse(std::string status);
		// Handle GET (non-CGI)
		int					readFullFileInBuffer(std::string& fullPath, size_t fileSize, char* buffer);
		int					makeFileResponse(std::string& fullPath, size_t fileSize, std::string status);
		std::string			getResponseBodyType(std::string& fullPath);
		int					handleGet();
		// Handle POST/PUT/DELETE (non-CGI)
		int					writeFullBufferInFile(std::string& fullPath, size_t bufferSize, char *buffer);
		int					uploadFile(std::string& fullPath, unsigned long fileSize, char* fileContent, bool makeResponse);
		int					handlePostRaw();
		int					handlePost();
		int					handlePut();
		int					handleDeleteOnDir(std::string& fullPath);
		int					handleDelete();
		// Handle POST/PUT on multipart bodies
		char*				extractMultipartBoundary(size_t& boundarySize);
		int					handlePostOnePart(std::string& dirPath, char* part, size_t partSize);
		void				divideMultipart(std::string& dirPath, char* boundary, size_t boundarySize);
		int					handlePostMultipart();
		// Handle GET on directories
		int					generateDefaultIndexHtml(std::string& fullPath, std::string& body);
		int					makeAutoindexResponse(std::string& fullPath);
		int					tryServeIndex(std::string& fullPath);
		int					handleGetOnDir();
		// Produce response
		int					handleNotCGI();
		int					redirectPath();
		bool				isCGIRequest();

};

#endif
