/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: administyrateur <administyrateur@studen    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 16:08:40 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/10 20:00:53 by administyra      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../hpp_files/webserv.hpp"

/////////////////////////////
// CANONICAL ORTHODOX FORM //
/////////////////////////////

Request::Request(t_mainSocket& mainSocket) : _mainSocket(mainSocket)
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

////////////////////////////////
// HEADERS AND BODY RECEPTION //
////////////////////////////////

/*
// Checks whether the request in <buffer> is complete,
// which means the double linebreak '\r\n\r\n' and either :
// 		\ there is no "Content-Length" (no body)
// 		\ there is a "Content-Length" and the part after double linebreak (the body)
// 		  has size exactly equal to that content length
bool	Request::requestIsComplete(const std::string& buffer)
{
	size_t	header_end;
	size_t	content_length;

	header_end = buffer.find("\r\n\r\n");
	if (header_end == std::string::npos)
		return (false);
	content_length = buffer.find("Content-Length:");
	// This detects the POST method, which contains a body
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

		// If we have received enough data (the whole body), then this is true.
		if (buffer.size() >= header_end + 4 + length)
			return (true);
		return (false);
	}
	//For GET and DELETE methods
	return (true);
}
*/

// Both buffers have size SOCKREAD_BUFFER_SIZE,
// <bufferSize> is only the number of bytes read in the socket and put in <buffer>
int	Request::containsHeadersEnd(char *lastBuffer, char* buffer, size_t bufferSize)
{
	size_t		ind;
	size_t		indInNeedle;
	size_t		sizeLastBuffer;
	size_t		sizeCombined;
	char*		combined;
	const char*	needle = "\r\n\r\n";

	sizeLastBuffer = strlen(lastBuffer);
	sizeCombined = sizeLastBuffer + bufferSize;
	combined = ft_strjoinDefsize(lastBuffer, buffer, bufferSize);
	ind = 0;
	while (ind < sizeCombined)
	{
		if (combined[ind] == '\0')
			return (-2);
		indInNeedle = 0;
		while (ind < sizeCombined && indInNeedle < 4 && combined[ind + indInNeedle] == needle[indInNeedle])
			indInNeedle++;
		if (indInNeedle == 4 && (ind + indInNeedle - 1 - sizeLastBuffer >= 0))
			return (ind + indInNeedle - 1 - sizeLastBuffer);
		ind++;
	}
	return (-1);
}

void	Request::allocateBody()
{
	std::cout << "[Req::allocate] allocated body of " << this->_bodySize << " bytes for request\n";
	this->_body = new char[this->_bodySize];
	bzero(this->_body, this->_bodySize);
	this->_nReceivedBodyBytes = 0;
}

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

// <extractFromUrl> and <extractFromHost> can't encounter an error
bool	Request::parse(const std::string& rawHeaders)
{
	if (this->parseFirstLine(rawHeaders))
	{
		std::cout << "[Req::parse] Error : Invalid request first line\n";
		return (1);
	}
	if (this->parseHeaders(rawHeaders))
	{
		std::cout << "[Req::parse] Error : Invalid request header lines\n";
		return (1);
	}
	this->extractFromURL();
	this->extractFromHost();
	this->_parsed = true;
	std::cout << "[Req::parse] Request successfully parsed\n";
	return (0);
}

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

// Checks that mandatory headers are present
// #f : add checks for Content-Length and Content-Type when there is a body (POST)
bool	Request::checkHeaders()
{
	if (this->_headers.count("Host") == 0)
	{
		std::cout << "[Req::parseHeaders] Error : Request lacks 'host' header\n";
		return (1);
	}
	if (this->_hasBody && this->_headers.count("Content-Type") == 0)
	{
		std::cout << "[Req::parse] Error : Request with body lacks Content-Type header\n";
		return (1);
	}
	if (!(this->_hasBody) && this->_headers.count("Content-Type") > 0)
	{
		std::cout << "[Req::parse] Error : Request without body has Content-Type header\n";
		return (1);
	}
	return (0);
}

// For each header line, inserts it into map <_headers>
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
	if (this->_headers.count("Content-Length") > 0)
	{
		this->_hasBody = 1;
		this->_bodySize = static_cast<unsigned long>(strtol(this->_headers["Content-Length"].c_str(), NULL, 10));
	}
	if (this->checkHeaders())
		return (1);
	return (0);
}

//////////////////////
// SPECIFIC PARSING //
//////////////////////

// Retrieves from requested resource URL in <_path> the non-mandatory elements
// 		\ file extension (after a '.')
// 		\ path info (following the file name)
// 		\ query string (last element, after a '?')
// that will be needed if the request is for a CGI script
// #f : check if query string can contain a '.', this would create pbs
void	Request::extractFromURL()
{
	unsigned long		indExtension;
	unsigned long		indEndExtension;
	unsigned long		indEndPathInfo;

	this->_extension = "";
	this->_pathInfo = "";
	this->_queryString = "";

	indExtension = this->_path.find('.');
	if (indExtension == std::string::npos) // No filename ending with an extension (so it cannot be a CGI)
		return ;
	indEndExtension = this->_path.find_first_of("/?", indExtension);
	if (indEndExtension == std::string::npos) // nothing after the filename (no path info, no query string)
	{
		this->_extension = this->_path.substr(indExtension);
		this->_filePath = this->_path;
		return ;
	}
	this->_extension = this->_path.substr(indExtension, indEndExtension - indExtension);
	this->_filePath = this->_path.substr(0, indEndExtension);

	indEndPathInfo = this->_path.find('?', indEndExtension);
	if (indEndPathInfo == indEndExtension) // filename immediately followed by query string
		this->_queryString = this->_path.substr(indEndPathInfo + 1);
	else if (indEndPathInfo != std::string::npos) // filename followed by path info, then query string
	{
		this->_pathInfo = this->_path.substr(indEndExtension, indEndPathInfo - indEndExtension);
		this->_queryString = this->_path.substr(indEndPathInfo + 1);
	}
	else // filename followed by only a path info
	{
		this->_pathInfo = this->_path.substr(indEndExtension);
		this->_queryString = "";
	}
}

// Retrieves from 'host' header field the information that the client used
// to send its request to this server
// 		\ a server name OR an IP address, before the ':'
// 		\ a port number (mandatory if different from scheme's default port,
// 			here scheme is HTTP so default port if not mentioned is 80)
void	Request::extractFromHost()
{
	std::string			host = this->_headers[std::string("Host")];
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
	else // for all remaining hostnames, they must be domain names, so IP address will be the default
	// Convert substring for port number (if there is one) into port number in <t_portaddr> struct
		this->_hostPortaddr.first = this->_mainSocket.portaddr.first;
	if (hostPort.size() > 0)
		this->_hostPortaddr.second = static_cast<uint16_t>(strtol(hostPort.c_str(), NULL, 10));
	else
		this->_hostPortaddr.second = 80;
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
		<< "\textension " << this->_extension << "\n"
		<< "\tpathInfo " << this->_pathInfo << "\n"
		<< "\tqueryString " << this->_queryString << "\n";
	std::cout << "Information on body :\n"
		<< "\thasBody " << this->_hasBody << "\n"
		<< "\tbodySize " << this->_bodySize << "\n";
	if (this->_hasBody && (this->_headers["Content-Type"] == "application/x-www-form-urlencoded"
		|| this->_headers["Content-Type"] == "multipart/form-data"
		|| this->_headers["Content-Type"] == "text/plain"
		|| 1)) // #f : to remove once tests are done
	{
		std::cout << ">\n< BODY\n";
		for (ind = 0; ind < this->_bodySize; ind++)
			std::cout << this->_body[ind];
	}
	std::cout << "\n>\033[0m\n";
}
