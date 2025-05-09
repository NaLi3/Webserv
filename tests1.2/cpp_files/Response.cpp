#include "Response.hpp"

Response::Response( Request& request, int clientSocket, t_mainSocket& mainSocket, std::vector<t_vserver>& vservers,
	std::set<int>& vservIndexes) : _request(request), _clientSocket(clientSocket), _mainSocket(mainSocket)
{
	std::cout << "Response constructor called\n";
	this->_vserv = this->identifyVserver(vservers, vservIndexes);
	this->_location = this->identifyLocation();
	std::cout << "Response for request directed to vserver " << *(this->_vserv->serverNames.begin());
	if (this->_location)
		std::cout << " at location " << this->_location->locationPath << "\n";
	else
		std::cout << " at root\n";
}

Response::~Response( void )
{
	std::cout << "Response destructor called\n";
}

std::string&	Response::getResponseBuffer(void)
{
	return (this->_responseBuffer);
}

t_vserver* Response::identifyVserver(std::vector<t_vserver>& vservers, std::set<int>& vservIndexes) {

	//Extract and parse host header.
	const std::string	hostHeader = this->_request.getHeaders().at("Host");
	std::string			hostName;
	std::string			hostPort;
	std::pair<uint32_t, uint16_t> clientAddr;
	in_addr addr;
	size_t sep;
	// Split host:port with the :
	clientAddr = this->_mainSocket.portaddr;
	sep = hostHeader.find(':');
	if (sep == std::string::npos)
		hostName = hostHeader;
	else
	{
		hostName = hostHeader.substr(0, sep);
		hostPort = hostHeader.substr(sep + 1);
	}
	// Convert hostName to IP if it's an IP or a localhost
	if ((hostName == "localhost" && inet_aton("127.0.0.1", &addr))
		|| inet_aton(hostName.c_str(), &addr))
	{
		clientAddr.first = ntohl(addr.s_addr);
		hostName.clear();
	}
	// Convert port string to integer for later
	if (!hostPort.empty())
		clientAddr.second = static_cast<uint16_t>(std::strtol(hostPort.c_str(), nullptr, 10));
	std::cout << "Host string in request: " << hostHeader
			<< " -> hostName: " << hostName
			<< " + hostPort: " << hostPort
			<< " -> portaddr: (" << clientAddr.first << ", " << clientAddr.second << ")\n";
	// Try matching virtual servers
	for (std::set<int>::iterator it = vservIndexes.begin(); it != vservIndexes.end(); ++it)
	{
		t_vserver& vserver = vservers[*it];
		bool portMatch = false;

		for (std::set<std::pair<uint32_t, uint16_t> >::iterator paIt = vserver.portaddrs.begin();
			paIt != vserver.portaddrs.end(); ++paIt)
			{
			if (portaddrContains(*paIt, clientAddr))
			{
				portMatch = true;
				break;
			}
		}
		if (!portMatch)
			continue ;
		// No hostName? Then port match is enough
		if (hostName.empty())
			return (&vserver);
		// Match server_name
		for (std::set<std::string>::iterator nameIt = vserver.serverNames.begin(); nameIt != vserver.serverNames.end(); ++nameIt)
		{
			if (*nameIt == hostName)
				return (&vserver);
		}
	}
	// Otherwise return the first server in the list
	return (&vservers.front());
}

void	Response::handleGet(int clientSocket)
{
	// Find correct server location.
	t_location*	location = this->identifyLocation();
	if (!location)
	{
		this->handleError(this->_clientSocket, 404, "Page not Found");
		return;
	}
	std::string requestPath;
	std::string root;
	std::string fullPath;
	// Verify the request path, or if simple server access start at root.
	requestPath = this->_request.getPath();
	if (requestPath.empty() || requestPath[0] != '/')
		requestPath = "/";
	root = location->rootPath;
	fullPath = root + requestPath;
	char realRoot[PATH_MAX];
	char realFile[PATH_MAX];
	// Get the full root path and the full path to file.
	if (!realpath(root.c_str(), realRoot) || !realpath(fullPath.c_str(), realFile))
	{
		this->handleError(clientSocket, 404, "Page not Found");
		return;
	}
	// Verify that the file you want to access is in the allowed root.
	if (std::string(realFile).find(realRoot) != 0)
	{
		this->handleError(clientSocket, 403, "Forbidden");
		return;
	}
	//If the file that you want to access isnt a file or has wrong permissions
	struct stat fileStat;
	if (stat(realFile, &fileStat) != 0 || !S_ISREG(fileStat.st_mode))
	{
		this->handleError(clientSocket, 404, "Page not Found");
		return;
	}
	// If the server can't give you a reproduction of its content.
	std::ifstream file(realFile, std::ios::in | std::ios::binary);
	if (!file)
	{
		this->handleError(clientSocket, 500, "Internal server error");
		return;
	}
	size_t	fileSize = fileStat.st_size;
	char*	buffer = new char[fileSize];
	if (!buffer)
	{
		this->handleError(clientSocket, 500, "Internal server error");
		return;
	}
	file.read(buffer, fileSize);
	file.close();
	//Send the response to the server using the http format.
	std::ostringstream response;
	response << "HTTP/1.1 200 OK\r\n"
			 << "Content-Type: " << this->getSameType(fullPath) << "\r\n"
			 << "Content-Length: " << fileSize << "\r\n"
			 << "Connection: close\r\n"
			 << "\r\n";
	std::string headerString = response.str();
	send(clientSocket, headerString.c_str(), headerString.size(), 0);
	send(clientSocket, buffer, fileSize, 0);
	delete[] buffer;
}

void	Response::handlePost( int clientSocket )
{
	t_location* location = this->identifyLocation();
	if (!location)
	{
		this->handleError(clientSocket, 404, "Page Not Found");
		return;
	}
	std::string requestPath = this->_request.getPath();
	if (requestPath.empty() || requestPath[0] != '/')
		requestPath = "/";
	std::string root = location->rootPath;
	std::string fullPath = root + requestPath;
	char realRoot[PATH_MAX];
	char realFile[PATH_MAX];
	if (!realpath(root.c_str(), realRoot) || !realpath(fullPath.c_str(), realFile))
	{
		this->handleError(clientSocket, 404, "Page Not Found");
		return;
	}
	if (std::string(realFile).find(realRoot) != 0)
	{
		this->handleError(clientSocket, 403, "Forbidden");
		return;
	}
	// Get content length
	std::map<std::string, std::string> headers = this->_request.getHeaders();
	std::map<std::string, std::string>::iterator it = headers.find("Content-Length");
	if (it == headers.end())
	{
		this->handleError(clientSocket, 411, "Length Required");
		return;
	}
	size_t contentLength = static_cast<size_t>(strtol(it->second.c_str(), NULL, 10));
	if (contentLength > MAXBODYSIZE)
	{
		this->handleError(clientSocket, 413, "Payload Too Large");
		return;
	}
	// Get body from request as char*
	const char* rawBody = this->_request.getBody().c_str();
	if (!rawBody)
	{
		this->handleError(clientSocket, 400, "Bad Request");
		return;
	}
	bool fileExists = (access(fullPath.c_str(), F_OK) == 0);
	// Save body to file (you could customize the destination path)
	std::ofstream outfile(fullPath.c_str(), std::ios::binary);
	if (!outfile)
	{
		this->handleError(clientSocket, 500, "Internal Server Error");
		return;
	}
	outfile.write(rawBody, contentLength);
	outfile.close();
	// Send a success response
	std::string status;
	if (fileExists == false)
		status = "201 Created";
	else
		status = "200 OK";
	std::string response =
		"HTTP/1.1 " + status + "\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 17\r\n"
		"Connection: close\r\n"
		"\r\n";
	send(clientSocket, response.c_str(), response.size(), 0);
}

void	Response::handleDelete( int clientSocket )
{
	t_location*	location  = identifyLocation();
	if (!location)
	{
		this->handleError(clientSocket, 404, "Page Not Found");
		return;
	}
	std::string requestPath;
	std::string root;
	std::string fullPath;
	requestPath = this->_request.getPath();
	if (requestPath.empty() || requestPath[0] != '/')
		requestPath = "/";
	fullPath = root + requestPath;
	char realRoot[PATH_MAX];
	char realFile[PATH_MAX];
	if (!realpath(root.c_str(), realRoot) || !realpath(fullPath.c_str(), realFile))
	{
		this->handleError(clientSocket, 404, "Page Not Found");
		return;
	}
	if (std::string(realFile).find(realRoot) != 0)
	{
		this->handleError(clientSocket, 403, "Forbidden File");
		return;
	}
	struct stat fileStat;
	if (stat(realFile, &fileStat) != 0)
	{
		this->handleError(clientSocket, 404, "File Not Found");
		return;
	}
	if (!S_ISREG(fileStat.st_mode))
	{
		this->handleError(clientSocket, 403, "Cannot Delete Non-Regular File");
		return ;
	}
	if (remove(realFile) != 0)
	{
		this->handleError(clientSocket, 500, "Failed To Delete File");
		return ;
	}
	std::ostringstream response;
	response << "HTTP/1.1 200 OK\r\n"
			 << "Content-Length: 0\r\n"
			 << "Connection: close\r\n"
			 << "\r\n";
	send(clientSocket, response.str().c_str(), response.str().size(), 0);
}

// t_vserver*	Response::identifyVserver(std::vector<t_vserver>& vservers, std::set<int>& vservIndexes)
// {
// 	std::string						host = (*((this->_request.getHeaders()).find(std::string("Host")))).second;
// 	std::string						hostName = "";
// 	std::string						hostPort = "";
// 	std::pair<uint32_t, uint16_t>	hostPortaddr;
// 	unsigned long					posSeparator;
// 	in_addr							addr;

// 	posSeparator = host.find(':');
// 	if (posSeparator == std::string::npos)
// 		hostName = host;
// 	else
// 	{
// 		hostName = host.substr(0, posSeparator);
// 		hostPort = host.substr(posSeparator + 1);
// 	}
// 	if ((hostName.compare("localhost") == 0 && inet_aton("127.0.0.1", &addr))
// 		|| inet_aton(hostName.c_str(), &addr))
// 	{
// 		hostPortaddr.first = ntohl(addr.s_addr);
// 		hostName = "";
// 	}
// 	else
// 		hostPortaddr.first = this->_mainSocket.portaddr.first;
// 	if (hostPort.size() > 0)
// 		hostPortaddr.second = static_cast<uint16_t>(strtol(hostPort.c_str(), NULL, 10));
// 	std::cout << "Host string in request : " << host << " -> hostName " << hostName
// 		<< " + hostPort " << hostPort << " -> portaddr " << hostPortaddr << "\n";

// 	std::set<int>::iterator								vservInd;
// 	std::set<std::pair<uint32_t, uint16_t> >::iterator	vservPortaddr;
// 	std::set<std::string>::iterator						vservName;
// 	bool												portaddrMatch;
// 	bool												nameMatch;

// 	for (vservInd = vservIndexes.begin(); vservInd != vservIndexes.end(); vservInd++)
// 	{
// 		portaddrMatch = 0;
// 		for (vservPortaddr = vservers[*vservInd].portaddrs.begin();
// 			vservPortaddr != vservers[*vservInd].portaddrs.end(); vservPortaddr++)
// 		{
// 			if (portaddrContains(*vservPortaddr, hostPortaddr))
// 			{
// 				portaddrMatch = 1;
// 				break;
// 			}
// 		}
// 		if (!portaddrMatch)
// 			continue;
// 		if (hostName.size() == 0)
// 			return (&(vservers[*vservInd]));
// 		nameMatch = 0;
// 		for (vservName = vservers[*vservInd].serverNames.begin();
// 			vservName != vservers[*vservInd].serverNames.end(); vservName++)
// 		{
// 			if (hostName.compare(*vservName) == 0)
// 			{
// 				nameMatch = 1;
// 				break;
// 			}
// 		}
// 		if (nameMatch)
// 			return (&(vservers[*vservInd]));
// 	}
// 	return (&(*(vservers.begin())));
// }

std::string	Response::getSameType( const std::string& path )
{
	if (ends_with(path, ".html"))
		return ("text/html");
	if (ends_with(path, ".jpg") || ends_with(path, ".jpeg"))
		return ("image/jpeg");
	if (ends_with(path, ".png"))
		return ("image/png");
	if (ends_with(path, ".css"))
		return ("text/css");
	if (ends_with(path, ".js"))
		return ("application/javascript");
	return ("application/octet-stream");
}

void	Response::handleError( int clientSocket, int code, const std::string& message )
{
	std::ostringstream response;
	response << "HTTP/1.1 "<< code << " " << message << "\r\n"
			 << "Content-Type: text/plain\r\n"
			 << "Content-Length: " << message.size() << "\r\n"
			 << "Connection: close\r\n"
			 << "\r\n"
			 << message;
	send(clientSocket, response.str().c_str(), response.str().size(), 0);
}

t_location*	Response::identifyLocation( void )
{
	t_location*		bestMatch;
	size_t			bestLength = 0;
	unsigned int	ind;
	std::string		locationPath;

	bestMatch = NULL;
	for (ind = 0; ind < this->_vserv->locations.size(); ind++)
	{
		locationPath = this->_vserv->locations[ind].locationPath;
		if (this->_request.getPath().compare(0, locationPath.size(), locationPath) == 0)
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

void	Response::extractExtension()
{
	unsigned long		indExtension;
	unsigned long		indEndExtension;
	std::string			path;


	path = this->_request.getPath();
	indExtension = path.find('.');
	if (indExtension == std::string::npos)
	{
		this->_extension = "";
		return ;
	}
	indEndExtension = path.find('/', indExtension);
	this->_extension = path.substr(indExtension, indEndExtension - indExtension);
	std::cout << "Extracted extension from path " << path << " : " << this->_extension << "\n";
}

bool	Response::isCGIRequest()
{
	std::map<std::string, std::string>::iterator	cgiIt;

	if (!(this->_location) || this->_location->cgiExtensions.size() == 0)
		return (0);
	for (cgiIt = this->_location->cgiExtensions.begin();
		cgiIt != this->_location->cgiExtensions.end(); cgiIt++)
	{
		if (this->_extension.compare((*cgiIt).first) == 0)
		{
			this->_cgiScript = (*cgiIt).second;
			return (1);
		}
	}
	return (0);
}

int		Response::executeCGIScript()
{
	this->_responseBuffer = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nContent-Type: text/html\r\n\r\nCGI";
	return (0);
}

int	Response::produceResponse(void)
{
	std::cout << "=====\nGenerating response for request on virtual server with hostname "
		<< *(this->_vserv->serverNames.begin());
	if (this->_location)
		std::cout << " at location " << this->_location->locationPath << "\n";
	else
		std::cout << " at root\n";
	this->extractExtension();
	if (this->isCGIRequest())
	{
		std::cout << "Request for CGI\n";
		return (this->executeCGIScript());
	}
	else
	{
		if (this->_request.getMethod() == "GET")
			handleGet(this->_clientSocket);
		std::cout << "Request without CGI\n"; // #f : join with handling of non-CGI request
		this->_responseBuffer = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nBASE";
		return (0);
	}
}

bool ends_with(const std::string& str, const std::string& suffix)
{
	if (suffix.size() > str.size())
		return (false);
	return (std::equal(suffix.rbegin(), suffix.rend(), str.rbegin()));
}
