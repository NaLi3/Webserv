/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilevy <ilevy@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 16:08:40 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/15 07:54:55 by ilevy            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../hpp_files/webserv.hpp"

/////////////////////////////
// CANONICAL ORTHODOX FORM //
/////////////////////////////

// Parameter <t_mainSocket> is a reference to the main socket
// on which the client making this request was first met,
// 		useful to give a default value to the request 'host IP address'
// 			(when the 'host' header of the request uses a domain name and not an IP)
// 		<=> the server IP address used by server to communicate with the client
Request::Request( t_mainSocket& mainSocket, int socket_fd ) : _mainSocket(mainSocket), _socketFd(socket_fd)
{
	std::cout << "Constructor for Request\n";
	this->_body = NULL;
	this->_hasBody = 0;
	this->_bodySize = 0;
}

Request::~Request()
{
	std::cout << "Destructor for Request\n";
	if (this->_body)
		delete[] this->_body;
}

///////////////
// ACCESSORS //
///////////////

// This class' member attributes are public, since this is a parsing class with no logic

////////////////////////////////
// HEADERS AND BODY RECEPTION //
////////////////////////////////

// Searches <lastBuffer> and <buffer> for a request separator '\r\n\r\n'
// 		\ takes 2 buffers as arguments ('\r\n\r\n' may begin in first one and end in second one)
// 		\ assembles the buffers into a single `char*` <combined> with <strJoinDefsize>
// 			(since <buffer> may contain some '\0', its size must be given as arg)
// 		\ uses <memFind> to search the bytes of <combined> for the needle '\r\n\r\n'
// 			~ if no match, returns -1
// 			~ if found, but the last '\n' is in <lastBuffer> instead of <buffer>, returns -2
// 				(should never happen, since <lastBuffer> was already searched, as <buffer>, during last call)
// 			~ if found, returns the index, relative to <buffer>, of the last '\n'
// /!\ Both buffers can hold SOCKREAD_BUFFER_SIZE bytes,
// 		<bufferSize> (<= SOCKREAD_BUFFER_SIZE) is the effective
// 		number of bytes read in the socket and put in <buffer>
ssize_t	Request::containsHeadersEnd(char *lastBuffer, char* buffer, size_t bufferSize)
{
	ssize_t		ind;
	size_t		sizeLastBuffer;
	size_t		sizeCombined;
	char*		combined;
	const char*	needle = "\r\n\r\n";

	sizeLastBuffer = strlen(lastBuffer);
	sizeCombined = sizeLastBuffer + bufferSize;
	combined = ft_strjoinDefsize(lastBuffer, buffer, bufferSize);
	ind = memFind(combined, sizeCombined, needle, 4);
	delete[] combined;
	if (ind == -1)
		return (-1);
	else if (ind + 3 - sizeLastBuffer < 0)
		return (-2);
	else
		return (ind + 3 - sizeLastBuffer);
}

// Allocates the `char*` buffer to store the body bytes of this request
// /!\ Allocates 1 more byte than necessary for <_bodySize>
// 	   to be safe in case of use on <_body> of <strcpy> or another function that would want to add '\0'
void	Request::allocateBody()
{
	std::cout << "[Req::allocate] allocated body of " << this->_bodySize << " bytes for request\n";
	this->_body = new char[this->_bodySize + 1];
	bzero(this->_body, this->_bodySize + 1);
	this->_nReceivedBodyBytes = 0;
}

// Appends <bufferSize> bytes from <buffer> to the <_body>,
// using <_nReceivedBodyBytes> to keep track of the position in <_body>
void	Request::appendToBody(char *buffer, size_t bufferSize)
{
	size_t	ind;

	ind = 0;
	while (ind < bufferSize)
	{
		this->_body[this->_nReceivedBodyBytes + ind] = buffer[ind];
		ind++;
	}
	this->_nReceivedBodyBytes += bufferSize;
}

/////////////////////
// GENERAL PARSING //
/////////////////////

// Parses headers contained in string <rawHeaders>
// 		\ a first line with URL and method type (GET, POST...)
// 		\ other lines with headers
// Also calls <extractFromUrl> and <extractFromHost> for specific parsing,
// which cannot encounter an error
bool	Request::parse(const std::string& rawHeaders)
{
	if (this->parseFirstLine(rawHeaders))
		return (logError("[Req::parse] Error : Invalid request first line", 0));
	if (this->parseHeaders(rawHeaders))
		return (logError("[Req::parse] Error : Invalid request header lines", 0));
	if (this->_path.empty() || this->_path[0] != '/' || this->_headers["Host"].empty())
		return (logError("[Req::parse] Error : Invalid request path/host fields", 0));
	this->extractFromURL();
	this->extractFromHost();
	this->_parsed = true;
	std::cout << "[Req::parse] Request successfully parsed\n";
	return (0);
}

// #f : generate error 505 on other Versions than HTTP/1.1
// Retrieves from the request's first line : the method name, resource URI, and HTTP version
bool	Request::parseFirstLine(const std::string& rawHeaders)
{
	size_t			line_end;
	size_t			method_end;
	size_t			path_end;
	std::string		request_line;

	line_end = rawHeaders.find("\r\n");
	if (line_end == std::string::npos)
		return (1);
	request_line = rawHeaders.substr(0,line_end);

	method_end = request_line.find(' ');
	if (method_end == std::string::npos)
		return (1);
	this->_method = request_line.substr(0, method_end);

	path_end = request_line.find(' ', method_end + 1);
	if (path_end == std::string::npos)
		return (1);

	this->_path = request_line.substr(method_end + 1, path_end - method_end - 1);
	this->_httpVersion = request_line.substr(path_end + 1);
	return (0);
}

// Checks that mandatory header 'host' is present,
// and that other headers are coherent with request method and body presence
bool	Request::checkHeaders()
{
	if (this->_headers.count("Host") == 0)
	{
		std::cout << "[Req::parse] Error : Request lacks 'host' header\n";
		return (1);
	}
	if (this->_hasBody && this->_method == "GET")
	{
		std::cout << "[Req::parse] Error : Request GET with body\n";
		return (1);
	}
	if (this->_hasBody && this->_headers.count("Content-Type") == 0)
	{
		std::cout << "[Req::parse] Error : Request with body lacks Content-Type header\n";
		return (1);
	}
	if (this->_headers.count("Content-Length") && this->_headers.count("Transfer-Encoding"))
	{
		std::cout << "[Req::parse] Error: Chunked Request contains Unchunked body\n";
		return (1);
	}
	if (this->_headers["Transfer-Encoding"] == "chunked")
	{
		if (!this->parseChunkedBody(this->_socketFd))
			return (1);
	}
	if (!(this->_hasBody) && this->_headers.count("Content-Type") > 0)
	{
		std::cout << "[Req::parse] Error : Request without body has Content-Type header\n";
		return (1);
	}
	return (0);
}

// For each header line, parses it and inserts it into map <_headers>,
// then sets <_hasBody> and <_bodySize> if header 'Content-Length' is present
bool	Request::parseHeaders(const std::string& rawHeaders)
{
	size_t		line_start;
	size_t		line_end;
	size_t		colon_pos;
	std::string	line_content;

	line_start = rawHeaders.find("\r\n") + 2;
	while (line_start < rawHeaders.size())
	{
		line_end = rawHeaders.find("\r\n", line_start);
		if (line_end == std::string::npos)
			break;
		line_content = rawHeaders.substr(line_start, line_end - line_start);
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
	if (!this->checkHeaders())
		return (0);
	if (this->_headers["Transfer-Encoding"] == "chunked")
	{
		this->_hasChunked = 1;
		this->_bodySize = 0;
	}
	if (this->_headers.count("Content-Length") > 0)
	{
		this->_hasBody = 1;
		this->_bodySize = static_cast<unsigned long>(strtol(this->_headers["Content-Length"].c_str(), NULL, 10));
	}
	return (1);
}

// This function looks through the chunked body line by line, searching for the trailing \r\n to remove and add to the body.
// The size of the body is modified through execution and memory is reallocated upon each loop for as long as content is found using chunkBuffer and newBody.
bool	Request::parseChunkedBody( int socket_fd )
{
	this->_body = NULL;
	this->_bodySize = 0;
	while (true)
	{
		std::string line = this->readLine(socket_fd);
		if (line.empty())
			return (false);
		if (line.size() >= 2 && line.substr(line.size() - 2) == "\r\n")
			line = line.substr(0, line.size() - 2);
		size_t	chunkSize = strtoul(line.c_str(), NULL, 16);
		if (chunkSize == 0)
			break ;
		char* chunkBuffer = new char[chunkSize];
		size_t	totalRead = 0;
		while (totalRead < chunkSize)
		{
			ssize_t bytes = recv(socket_fd, chunkBuffer + totalRead, chunkSize - totalRead, 0);
			if (bytes <= 0)
			{
				delete[] chunkBuffer;
				return (false);
			}
			totalRead += bytes;
		}
		char* newBody = new char[this->_bodySize + chunkSize];
		if (this->_body)
		{
			memcpy(newBody, this->_body, this->_bodySize);
			delete[] this->_body;
		}
		memcpy(newBody + this->_bodySize, chunkBuffer, chunkSize);
		delete[] chunkBuffer;
		this->_body = newBody;
		this->_bodySize += chunkSize;
		std::string crlf = readLine(socket_fd);
		if (crlf != "\r\n" && crlf != "")
			return (false);
	}
	this->_parsed = true;
	return (true);
}

//////////////////////
// SPECIFIC PARSING //
//////////////////////

// #f : check if we have problem with path containing hidden directories (begin with '.')
// Retrieves from requested resource URL in <_path> the elements
// 		\ <_filePath> : the path to the requested resource only
// 		\ bool <_toDir> : whether that path ends with '/', ie. leads to a directory
// 		\ <_extension> : the request file extension (after a '.') ; non-mandatory
// 			(but if there is no extension, then there can be no pathInfo)
// 		\ <_pathInfo> : a supplementary path (following the file name) ; non-mandatory
// 		\ <_queryString> : query parameters (last element, after a '?') ; non-mandatory
// that will be needed if the request is for a CGI script
void	Request::extractFromURL()
{
	unsigned long	ind1;
	unsigned long	ind2;

	// Default values
	this->_toDir = 0;
	this->_filePath = "";
	this->_extension = "";
	this->_pathInfo = "";
	this->_queryString = "";

	ind1 = this->_path.find('?'); // searches position of beginning of query string
	if (ind1 != std::string::npos && ind1 + 1 < this->_path.size())
		this->_queryString = this->_path.substr(ind1 + 1);
	this->_filePath = this->_path.substr(0, ind1); // temporary assignment

	ind1 = this->_filePath.find('.'); // searches position of file extension
	if (ind1 != std::string::npos) // if there is an extension
	{
		ind2 = this->_filePath.find('/', ind1);
		if (ind2 != std::string::npos)
			this->_pathInfo = this->_filePath.substr(ind2);
		this->_extension = this->_filePath.substr(ind1, ind2 - ind1);
		this->_filePath = this->_filePath.substr(0, ind2);
	}
	else // if there is no extension, it may be that the path ends with '/'
		this->_toDir = (this->_filePath[this->_filePath.size() - 1] == '/');
}

// Retrieves from 'host' header field the information that the client used
// to send its request to this server
// 		\ a server name OR an IP address, before the ':'
// 		\ a port number (mandatory if different from scheme's default port,
// 			here scheme is HTTP so default port if not mentioned is 80)
void	Request::extractFromHost()
{
	std::string			host = this->_headers["Host"];
	std::string			hostPort;
	unsigned long		posSeparator;
	in_addr				addr;

	// Separate substring for hostname (domain name or IP address), and substring for port number
	this->_hostName = "";
	hostPort = "";
	posSeparator = host.find(':');
	if (posSeparator == std::string::npos)
		this->_hostName = host;
	else
	{
		this->_hostName = host.substr(0, posSeparator);
		hostPort = host.substr(posSeparator + 1);
	}
	// Convert substring for hostname into IP address in <t_portaddr> struct,
	// and keep <Request._hostName> only if it was a domain name
	if ((this->_hostName.compare("localhost") == 0 && inet_aton("127.0.0.1", &addr))
		|| inet_aton(this->_hostName.c_str(), &addr)) // for all hostnames that are IP addresses, or 'localhost'
	{
		this->_hostPortaddr.first = ntohl(addr.s_addr);
		this->_hostName = ""; // hostname erased, since it was not a domain name
	}
	else // for all remaining hostnames, they must be domain names, so we use the default IP address from mainSocket
		this->_hostPortaddr.first = this->_mainSocket.portaddr.first;
	// Convert substring for port number (if there is one) into port number in <t_portaddr> struct
	if (hostPort.size() > 0)
		this->_hostPortaddr.second = static_cast<uint16_t>(strtol(hostPort.c_str(), NULL, 10));
	else
		this->_hostPortaddr.second = 80; // default port number 80 since Webserv is an HTTP server
}

//////////////////
// REDIRECTIONS //
//////////////////

// Public method to be called when a local redirection occurs,
// which modifies only 1 element inside the request : the path to requested resource
// Updates the member string <_path> and re-parses it with <extractFromURL>
int		Request::redirectPath(std::string newPath)
{
	if (newPath.empty() || newPath[0] != '/')
		return (1);
	this->_path = newPath;
	this->extractFromURL();
	return (0);
}

///////////
// UTILS //
///////////

// Logs request components
void	Request::logRequest(void)
{
	std::map<std::string, std::string>::iterator	headerIt;
	unsigned long									ind;

	std::cout << "\033[33m< REQUEST PARSED\n";
	std::cout << "Method: " << this->_method << "\n";
	std::cout << "Path: " << this->_path << "\n";
	std::cout << "Headers:\n";
	for (headerIt = this->_headers.begin(); headerIt != this->_headers.end(); ++headerIt)
		std::cout << "\t" << headerIt->first << ": " << headerIt->second << "\n";
	std::cout << "Extracted from host header :\n"
		<< "\thostName " << this->_hostName << "\n"
		<< "\tportaddr " << this->_hostPortaddr << "\n";
	std::cout << "Extracted from path " << this->_path << " :\n"
		<< "\tfilePath " << this->_filePath << "\n"
		<< "\ttoDir " << this->_toDir << "\n"
		<< "\textension " << this->_extension << "\n"
		<< "\tpathInfo " << this->_pathInfo << "\n"
		<< "\tqueryString " << this->_queryString << "\n"
		<< "\thasChunked" << this->_hasChunked << "\n";
	std::cout << "Information on body :\n"
		<< "\thasBody " << this->_hasBody << "\n"
		<< "\tbodySize " << this->_bodySize << "\n";
	if (this->_hasBody && (this->_headers["Content-Type"] == "application/x-www-form-urlencoded"
		|| this->_headers["Content-Type"].compare(0, 19, "multipart/form-data") == 0
		|| this->_headers["Content-Type"] == "text/plain"))
	//if (1) // #f : to remove once tests are done
	{
		std::cout << ">\n< BODY\n";
		for (ind = 0; ind < this->_bodySize; ind++)
			std::cout << this->_body[ind];
	}
	std::cout << "\n>\033[0m\n";
}

std::string	Request::readLine( int socket_fd )
{
	std::string line;
	char	c;
	while (recv(socket_fd, &c, 1, 0) == 1)
	{
		line += c;
		if (line.size() >= 2 
			&& line[line.size() - 2] == '\r' 
			&& line[line.size() - 1] == '\n')
			break ;
	}
	return (line);
}