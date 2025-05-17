/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: administyrateur <administyrateur@studen    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 16:08:40 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/17 19:30:09 by administyra      ###   ########.fr       */
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
Request::Request(t_mainSocket& mainSocket) : _mainSocket(mainSocket)
{
	std::cout << "Constructor for Request\n";
	this->_body = NULL;
	this->_nReceivedBodyBytes = 0;
	this->_hasBody = 0;
	this->_chunkedBody = 0;
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

/////////////////////////////////////
// VSERVER/LOCATION IDENTIFICATION //
/////////////////////////////////////

// Returns a pointer to the virtual server (in the list <vservers>, maintained by <Server>)
// that should generate the response for the request in <this._request>
// Parameter <versers> is the list of all virtual servers,
// parameter <vservIndexes> gives the indexes of the virtual servers that could match
// 		(specifically, the indexes of the virtual servers handles by the server mainSocket
// 		 on which the client making this request was first received)
// Virtual server match must respect 2 conditions :
// 		\ virtual server must listen on a portaddr that is (or contains)
// 		  the portaddr on which the request was received, ie. <this._request.hostPortaddr>
// 		\ the virtual server must have a 'server name' that is
// 		  the domain name given in the request's host header, ie. <this._request.hostName>
// If no match found, returns the virtual server with the first index in <vservIndexes>
t_vserver*	Request::identifyVserver(std::vector<t_vserver>& vservers, std::set<int>& vservIndexes)
{
	std::set<int>::iterator					vservInd;
	t_vserver*								vserv;
	std::set<t_portaddr >::iterator			vservPortaddr;
	std::set<std::string>::iterator			vservName;
	bool									portaddrMatch;
	bool									nameMatch;

	std::cout << "[Res::idVserv] Finding vserver responsible for request with hostName "
		<< this->_hostName << " received on " << this->_hostPortaddr << "\n";
	// try matching host description in request with virtual servers
	for (vservInd = vservIndexes.begin(); vservInd != vservIndexes.end(); vservInd++)
	{
		vserv = &(vservers[*vservInd]);
		// Check if 1 of virtual server's portaddrs matches host portaddr
		portaddrMatch = 0;
		for (vservPortaddr = vserv->portaddrs.begin();
			vservPortaddr != vserv->portaddrs.end(); vservPortaddr++)
		{
			if (portaddrContains(*vservPortaddr, this->_hostPortaddr))
			{
				portaddrMatch = 1;
				break;
			}
		}
		// if no match, it cannot be this virtual server
		if (!portaddrMatch)
			continue;
		// if portaddr match and no hostName in request, then portaddr match is enough
		if (this->_hostName.size() == 0)
		{
			std::cout << "\tFound portaddr match, no hostName -> vserver is " << vserv << "\n";
			return (vserv);
		}
		// if portaddr match and hostName defined,
		// then check if hostName matches 1 of the virtual server's name
		nameMatch = 0;
		for (vservName = vserv->serverNames.begin();
			vservName != vserv->serverNames.end(); vservName++)
		{
			if (this->_hostName == *vservName)
			{
				nameMatch = 1;
				break;
			}
		}
		if (nameMatch)
		{
			std::cout << "\tFound portaddr match + hostName match -> vserver is " << vserv << "\n";
			return (vserv);
		}
	}
	// when no virtual server matches, return the first server in the list
	std::cout << "[Res::idVserv] no match found -> returning first possible vserver\n";
	return (&(vservers[*(vservIndexes.begin())]));
}

// Returns a pointer to the location, in the current virtual server,
// targeted by the request in <this._request>
// Loops through the virtual server's locations searching for a match
// between the location's path and the path given in request ;
// 		\ if location's path ends with '/', this ending '/' is not taken into account for match
// 		\ if multiple locations match the one with the longest location path prevails
// 		\ if no match found, returns NULL (a request can target no location)
// Example : if request path is '/a/b/c.txt', locations are '/a/' '/a/b/' '/a/b/c/',
// 		then location '/a/b/' will be chosen (longest match)
// [cfg feature] define parameteres for specific 'routes'='locations'
t_location*	Request::identifyLocation()
{
	t_location*		bestMatch;
	size_t			bestLength = 0;
	unsigned int	ind;
	std::string		locationPath;
	std::string		filePath;

	bestMatch = NULL;
	for (ind = 0; ind < this->_vserv->locations.size(); ind++)
	{
		locationPath = this->_vserv->locations[ind].locationPath;
		if (locationPath != "/" && *(locationPath.end() - 1) == '/')
			eraseLastChar(locationPath);
		if (this->_filePath.compare(0, locationPath.size(), locationPath) == 0)
		{
			if (locationPath.size() > bestLength)
			{
				bestLength = locationPath.size();
				bestMatch = &(this->_vserv->locations[ind]);
			}
		}
	}
	return (bestMatch);
}

// Calls the two private methods that will identify the vserver and location targeted by request
// 		\ <vservIndexes> is (a reference to) the list of vservers listening
// 		  on the server portaddr on which the connection is occurring (<=> on which the request was received)
// 			-> necessary to identify which vserver should respond to the request
// 		\ <vservers> is (a reference to) a list of structures representing the virtual servers
// 			(the indexes in <mainSocket.vservIndexes> are positions in this list)
// The request may target no location, in that case <_location> is NULL
void	Request::identifyTargeted(std::vector<t_vserver>& vservers, std::set<int>& vservIndexes)
{
	this->_vserv = this->identifyVserver(vservers, vservIndexes);
	this->_location = this->identifyLocation();
}

////////////////////////////////
// HEADERS AND BODY RECEPTION //
////////////////////////////////

// Searches <previousBytes> and <buffer> for a request separator '\r\n\r\n'
// 		\ takes 2 arguments because '\r\n\r\n' may begin in <previousBytes> and end in <buffer>
// 			(since <buffer> may contain some '\0', its size must be given as arg)
// 		\ assembles the buffers into a single `char*` <combined> with <strJoinDefsize>
// 			by taking only the 3 last characters of <previousBytes>, and all of <buffer>
// 		\ uses <memFind> to search the bytes of <combined> for the needle '\r\n\r\n'
// 			~ if no match, returns -1
// 			~ if found, returns the index, relative to <buffer>, OF THE LAST '\n'
// 				(not the index of the first character of separator '\r', but the index of last character '\n' !)
// <bufferSize> (<= SOCKREAD_BUFFER_SIZE) is the effective number of bytes
// read in the socket and put in <buffer> (<buffer> itself can hold SOCKREAD_BUFFER_SIZE characters)
ssize_t	Request::containsHeadersEnd(std::string& previousBytes, char* buffer, size_t bufferSize)
{
	ssize_t		indSep;
	size_t		ind;
	size_t		sizeLastBytes;
	size_t		sizeCombined;
	char*		combined;
	const char*	needle = "\r\n\r\n";

	sizeLastBytes = std::min(3UL, previousBytes.size());
	sizeCombined = sizeLastBytes + bufferSize;
	combined = new char[sizeCombined + 1];
	bzero(combined, sizeCombined + 1);
	for (ind = 0; ind < sizeLastBytes; ind++)
		combined[ind] = previousBytes[previousBytes.size() - sizeLastBytes + ind];
	memcpy(combined + sizeLastBytes, buffer, bufferSize);
	indSep = memFind(combined, sizeCombined, needle, 4);
	delete[] combined;
	if (indSep == -1)
		return (-1);
	else
		return (indSep + 3 - sizeLastBytes);
}

// Allocates the `char*` buffer to store the body bytes of this request
// 		(only for non-chunked body, since for chunked body total size is unknown)
// /!\ Allocates 1 more byte than necessary for <_bodySize>
// 	   to be safe in case of use on <_body> of <strcpy> or another function that would want to add '\0'
void	Request::allocateBody()
{
	if (this->_chunkedBody == 0)
	{
		std::cout << "[Req::allocate] allocates body of " << this->_bodySize << " bytes for request\n";
		this->_body = new char[this->_bodySize + 1];
		bzero(this->_body, this->_bodySize + 1);
		this->_nReceivedBodyBytes = 0;
	}
}

// Appends <bufferSize> bytes from <buffer> to the <_body>,
// using <_nReceivedBodyBytes> to keep track of the position in <_body>,
// and returns 0 if body still needs bytes to be completed, or 1/2 if those bytes completed it
// In case of chunked body, handling of <buffer> is passed to <appendToChunkedBody>
// 		which may also return 3 in case of an error in chunked body
// 		or 4 if body too large for limit defined in cfg file
int	Request::appendToBody(char *buffer, size_t bufferSize)
{
	if (this->_chunkedBody)
		return (this->appendToChunkedBody(buffer, bufferSize));
	if (this->_nReceivedBodyBytes + bufferSize == this->_bodySize)
	{
		memcpy(this->_body + this->_nReceivedBodyBytes, buffer, bufferSize);
		this->_nReceivedBodyBytes += bufferSize;
		std::cout << "\t\t[Req::recBody] Received just enough bytes to complete body ("
			<< this->_nReceivedBodyBytes << " / " << this->_bodySize << ")\n";
		return (1);
	}
	else if (this->_nReceivedBodyBytes + bufferSize > this->_bodySize)
	{
		memcpy(this->_body + this->_nReceivedBodyBytes, buffer, this->_bodySize - this->_nReceivedBodyBytes);
		std::cout << "\t\t[Req::recBody] (Warning) Received more than enough bytes to complete body ("
			<< this->_nReceivedBodyBytes + bufferSize << " / " << this->_bodySize << ")\n";
		this->_nReceivedBodyBytes = this->_bodySize;
		return (2);
	}
	else
	{
		memcpy(this->_body + this->_nReceivedBodyBytes, buffer, bufferSize);
		this->_nReceivedBodyBytes += bufferSize;
		std::cout << "\t\t[CH::receiveHTTP] Received bytes for body, still not complete ("
			<< this->_nReceivedBodyBytes << " / " << this->_bodySize << ")\n";
		return (0);
	}
}

////////////////////////////
// CHUNKED BODY RECEPTION //
////////////////////////////

// Sets the values/flags for processing chunked body to their correct initial values
void	Request::initChunkedBody()
{
	this->_chunksTotalSize = 0;
	this->_bodySize = 0;
	this->_body = NULL;
	this->_chunkSepOffset = 0;
	this->_chunkSize = std::string::npos;
	this->_prevChunkHead = "";
	this->_nReceivedChunkBytes = 0;
	this->_lastChunkDone = 0;
}

// Updates the body buffer when a new chunk makes the old body size insufficient
// Allocates a new body of twice the required size (for margin)
// then copies the content of the old body to the new body (as in a <realloc>)
void	Request::adaptBodySizeToChunks()
{
	char	*newBody;

	if (this->_bodySize >= this->_chunksTotalSize)
		return ;
	std::cout << "\textending body to size " << this->_chunksTotalSize * 2 << "\n";
	newBody = new char[this->_chunksTotalSize * 2];
	bzero(newBody, this->_chunksTotalSize * 2);
	if (this->_body)
	{
		memcpy(newBody, this->_body, this->_nReceivedBodyBytes);
		delete[] this->_body;
	}
	this->_body = newBody;
	this->_bodySize = this->_chunksTotalSize * 2;
}

// Combines the previously read bytes in <this._prevChunkHead> and the bytes in <buffer>
// into a <haystack>, then searches <haystack> for separator "\r\n" and returns its index
ssize_t	Request::searchForChunkHead(char *buffer, size_t bufferSize)
{
	ssize_t			indInHaystack;
	const char*		needle = "\r\n";
	char*			haystack;
	size_t			haystackSize;

	haystackSize = this->_prevChunkHead.size() + bufferSize;
	haystack = new char[haystackSize + 1];
	bzero(haystack, haystackSize + 1);
	memcpy(haystack, this->_prevChunkHead.c_str(), this->_prevChunkHead.size());
	memcpy(haystack + this->_prevChunkHead.size(), buffer, bufferSize);
	indInHaystack = memFind(haystack, haystackSize, needle, 2);
	delete[] haystack;
	return (indInHaystack);
}

// Function called when next chunk size is unknown (`this_chunkSize == std::string::npos`)
// 		<=> the last received bytes of chunked body were bytes on a chunk head,
// 			and the <buffer> could contain separator "\r\n" indicating chunk head end
// Member variable <_prevChunkHead> contains all bytes of this chunk head received until now,
// these bytes are concatenated with the <buffer> bytes in <haystack>,
// then <haystack> is searched for chunk head end "\r\n"
// 		\ if not found <buffer> bytes are simply appended to <_prevChunkHead>,
// 				then function returns to wait for next bytes
// 		\ if found, next chunk size is extracted from bytes, <_body> is extended if needed,
// 			and potential additional bytes after "\r\n" are processed
// Error cases (function returns 3) :
// 		\ `(indInHaystack != -1 && ind + 1 < 0)` : separator found,
// 				but character '\n' is in <_prevChunkHead> instead of <buffer>
// 		\ `(indInHaystack == -1 && strlen(buffer) != bufferSize)` :
// 				there are '\0' characters inside a chunk head
// 		\ `chunksTotalSize > ...` : body larger than allowed by config file
// [cfg feature] : define max request body for a vserver/location
int	Request::processChunkHead(char *buffer, size_t bufferSize)
{
	ssize_t			indInHaystack;
	ssize_t			ind;

	indInHaystack = this->searchForChunkHead(buffer, bufferSize);
	ind = indInHaystack - this->_prevChunkHead.size();
	std::cout << "\tresult of search for chunk head end : in haystack "
		<< indInHaystack << ", in buffer " << ind << "\n";
	if ((indInHaystack != -1 && ind + 1 < 0)
		|| (indInHaystack == -1 && strlen(buffer) != bufferSize))
		return (3);
	if (indInHaystack == -1)
	{
		std::cout << "\tchunk head end not found in buffer\n";
		this->_prevChunkHead += std::string(buffer);
		return (0);
	}
	if (ind > 0) // <=> if the '\r' character is in <buffer> and at least at pos 1 in <buffer
		this->_prevChunkHead += std::string(buffer, buffer + ind);
	this->_chunkSize = strtoul(this->_prevChunkHead.c_str(), NULL, 16);
	std::cout << "\tfound chunk head end at index " << ind << " -> chunkSize " << this->_chunkSize << "\n";
	this->_prevChunkHead = "";
	this->_chunksTotalSize += this->_chunkSize;
	if (this->_chunksTotalSize > this->_vserv->maxRequestBodySize
		|| (this->_location && this->_chunksTotalSize > this->_location->maxRequestBodySize))
		return (4);
	this->adaptBodySizeToChunks();
	if (this->_chunkSize == 0)
		this->_bodySize = this->_chunksTotalSize;
	if (ind + 2 < static_cast<ssize_t>(bufferSize))
		return (this->appendToChunkedBody(buffer + ind + 2, bufferSize - ind - 2));
	else
		return (0);
}

// Processes a <buffer> containing bytes of a chunked body ("Transfer-Encoding: chunked")
// HTTP chunked bodies are made of any number of (chunk head + sep + chunk content + sep), where
// 		\ chunk head gives chunk content size in base 16
// 		\ sep is "\r\n"
// 		\ end of chunked body is indicated by last chunk of size 0
// General functioning :
// 		\ a lot of potential recursion : once some bytes are extracted 
// 		  for chunk head / chunk content / separator ("\r\n"), recurr on remaining bytes
// 			until there is no more remaining bytes (ret 0) / error occurs (ret 3) / body ends (ret 1,2)
// 		\ member variable <_chunkSize> means
// 			~ if defined : currently processing bytes from chunk content, chunk has this size
// 			~ if undefined (std::string::npos) : currently processing bytes from chunk head,
// 				once chunk head end found the chunk size will be extracted from chunk head
// 		\ member variable <_chunkSepOffset> means
// 			~ if 0 : bytes can be processed as usual according to <_chunkSize>
// 			~ if 1,2 : that much bytes constituting separator "\r\n" at a chunk's end are expected
// 				-> check that bytes are indeed "\r" or "\n", extract them, and recurr
// 		\ for end condition, member variable <_lastChunkDone> set to 1 when chunk of size 0 found,
// 			there remains only two bytes final "\r\n" to read (<_chunkSepOffset> = 2)
// 			then function can return 1 if no more bytes as expected, or 2 if bytes still present in buffer
// Function flow :
// 		\ check chunked body end condition, check recursion end condition
// 		\ extract separator bytes if necessary (`_chunkSepOffset > 0`)
// 		\ extract chunk head bytes if necessary (`_chunkSize == std::string::npos`)
// 		\ add buffer bytes to body, if more bytes than necessary to complete chunk,
// 			set <_chunkSepOffset> to expect "\r\n" and recurr on supplementary bytes
int	Request::appendToChunkedBody(char *buffer, size_t bufferSize)
{
	ssize_t		suppBytes;

	std::cout << "[Req::recChunk] on buffer of size " << bufferSize
		<< ", chunkSize " << this->_chunkSize << ",\n\tnReceived " << this->_nReceivedBodyBytes
		<< ", sepOffset " << this->_chunkSepOffset << ", last " << this->_lastChunkDone
		<< "\n\033[33m<\n" << buffer << "\n>\033[0m\n";
	if (bufferSize == 0 && this->_lastChunkDone)
		return (1);
	if (bufferSize == 0)
		return (0);
	if (this->_chunkSepOffset > 0)
	{
		if ((this->_chunkSepOffset == 2 && buffer[0] != '\r')
			|| (this->_chunkSepOffset == 1 && buffer[0] != '\n'))
			return (3);
		this->_chunkSepOffset--;
		return (this->appendToChunkedBody(buffer + 1, bufferSize - 1));
	}
	if (this->_chunkSize == std::string::npos)
		return (this->processChunkHead(buffer, bufferSize));
	if (this->_lastChunkDone)
	{
		std::cout << "[Req::recChunk] (Warning) Received bytes after last chunk\n";
		return (2);
	}
	suppBytes = this->_nReceivedBodyBytes + bufferSize - this->_chunksTotalSize;
	if (suppBytes < 0)
	{
		std::cout << "\tbytes appended to current chunk\n";
		memcpy(this->_body + this->_nReceivedBodyBytes, buffer, bufferSize);
		this->_nReceivedBodyBytes += bufferSize;
		return (0);
	}
	else
	{
		std::cout << "\tbytes completing current chunk\n";
		if (this->_chunkSize == 0)
			this->_lastChunkDone = 1;
		else
			memcpy(this->_body + this->_nReceivedBodyBytes, buffer, bufferSize - suppBytes);
		this->_nReceivedBodyBytes = this->_chunksTotalSize;
		this->_chunkSize = std::string::npos;
		this->_chunkSepOffset = 2;
		return (this->appendToChunkedBody(buffer + bufferSize - suppBytes, suppBytes));
	}
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
	if (this->_httpVersion != "HTTP/1.1")
		return (logError("[Req::parse] Error : Invalid HTTP protocol version", 0));
	if (this->parseHeaders(rawHeaders))
		return (logError("[Req::parse] Error : Invalid request header lines", 0));
	if (this->_path.empty() || this->_path[0] != '/' || this->_headers["Host"].empty())
		return (logError("[Req::parse] Error : Invalid request path/host fields", 0));
	if (this->invalidPath())
		return (logError("[Req::parse] Error : Path contains '.' or '..'", 0));
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

// Checks request headers coherence with request method and body presence
// Some specific checks :
// 		\ presence of host header
// 		\ if and only if presence of body, then presence of Content-Type
// 		\ presence of Transfer-Encoding incompatible with Content-Length ; must have value "chunked"
// Then sets flags <_hasBody> (+ size <_bodySize) and <_chunkedBody>
// according to headers Content-Length/Transfer-Encoding
bool	Request::checkHeaders()
{
	bool	hasBody = (this->_headers.count("Content-Length") || this->_headers.count("Transfer-Encoding"));

	if (this->_headers.count("Host") == 0)
		return (logError("[Req::parse] Error : Request lacks 'host' header", 0));
	if (hasBody && this->_method == "GET")
		return (logError("[Req::parse] Error : Request GET with body", 0));
	if (this->_headers.count("Transfer-Encoding") && this->_headers["Transfer-Encoding"] != "chunked")
		return (logError("[Req::parse] Error: Transfer-Encoding has value other than 'chunked'", 0));
	if (hasBody && this->_headers.count("Content-Type") == 0)
		return (logError("[Req::parse] Error : Request with body lacks Content-Type header", 0));
	if (!hasBody && this->_headers.count("Content-Type") > 0)
		return (logError("[Req::parse] Error : Request without body has Content-Type header", 0));
	if (this->_headers.count("Content-Length") && this->_headers.count("Transfer-Encoding"))
		return (logError("[Req::parse] Error: Chunked Request contains unchunked body", 0));
	
	if (this->_headers.count("Content-Length"))
	{
		this->_hasBody = 1;
		this->_bodySize = static_cast<unsigned long>(strtol(this->_headers["Content-Length"].c_str(), NULL, 10));
	}
	else if (this->_headers.count("Transfer-Encoding") && this->_headers["Transfer-Encoding"] == "chunked")
	{
		this->_hasBody = 1;
		this->_chunkedBody = 1;
		this->initChunkedBody();
	}
	return (0);
}

// For each header line, parses it and inserts it into map <_headers>,
// then calls <checkHeaders> to check headers coherence and detect body presence
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
	if (this->checkHeaders())
		return (1);
	return (0);
}

//////////////////////
// SPECIFIC PARSING //
//////////////////////

// Checks that the request's path does not contain :
// 		\ '.' (representing 'this directory')
// 		\ '..' (representing 'parent directory')
// 		\ '.name.ext' (hidden file or directory)
bool	Request::invalidPath()
{
	size_t		ind;

	ind = this->_path.find("/.");
	if (ind != std::string::npos)
		return (1);
	return (0);
}

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

// Public method to be called when a LOCAL redirection occurs, to modify :
// 		\ the request's path to resource = member string <_path>
// 			-> <_path> must be re-parsed with <extractFromURL>
// 		\ (optionnally) the request's method, eg. for redirection to error/index pages
// 			-> if the method become "GET", body and its headers must be removed
// Current implementation uses this in only 1 case : CGI resolving in local redirection
// /!\ This is not used for CLIENT redirections
// 	   (in this case, the client will send a new request with the correct path)
int		Request::redirectPath(std::string newPath, std::string newMethod)
{
	if (newPath.empty() || newPath[0] != '/')
		return (1);
	this->_path = newPath;
	this->extractFromURL();

	if (newMethod.empty())
		return (0);
	if (this->_method != "GET" && newMethod == "GET")
	{
		this->_hasBody = 0;
		if (this->_body)
		{
			delete[] this->_body;
			this->_body = NULL;
		}
	}
	this->_method = newMethod;
	return (0);
}

///////////////////////////////////////////
// EARLY CHECKS ON REQUEST ACCEPTABILITY //
///////////////////////////////////////////

// Checks on body size
// (called by <Client> before body is read, so cannot check for chunked body)
// [cfg feature] : define max request body for a vserver/location
int	Request::bodySizeForbidden()
{
	if (this->_hasBody && !(this->_chunkedBody)
		&& (this->_bodySize > this->_vserv->maxRequestBodySize
			|| (this->_location && this->_bodySize > this->_location->maxRequestBodySize)))
		return (1);
	return (0);
}

// Checks on methods allowed for a location
// [cfg feature] : define methods allowed for a location
int	Request::methodForbidden()
{
	if (!(this->_location) && this->_method != "GET")
		return (1);
	if (this->_location && this->_location->acceptedMethods.count(this->_method) == 0)
		return (1);
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
		<< "\tqueryString " << this->_queryString << "\n";
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
