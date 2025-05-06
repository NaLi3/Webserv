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
	t_location*	location = this->identifyLocation();
	if (!location)
	{
		std::cout << "Error 404: Page not found" << std::endl;
		return;
	}
	std::string requestPath;
	std::string root;
	std::string fullPath;
	requestPath = this->_request.getPath();
	if (requestPath.empty() || requestPath[0] != '/')
		requestPath = "/";
	root = location->rootPath;
	fullPath = root + requestPath;
	char realRoot[PATH_MAX];
	char realFile[PATH_MAX];
	if (!realpath(root.c_str(), realRoot) || !realpath(fullPath.c_str(), realFile))
	{
		std::cout << "Error 404: Page not found" << std::endl;
		return;
	}
	if (std::string(realFile).find(realRoot) != 0)
	{
		std::cout << "Error 403: Forbidden page" << std::endl;
		return;
	}
	struct stat fileStat;
	if (stat(realFile, &fileStat) != 0 || !S_ISREG(fileStat.st_mode))
	{
		std::cout << "Error 404: Page not found" << std::endl;
		return;
	}
	std::ifstream file(realFile, std::ios::in | std::ios::binary);
	if (!file)
	{
		std::cout << "Error 500: Internal Server Error" << std::endl;
		return;
	}
	std::ostringstream bodyStream;
	bodyStream << file.rdbuf();
	std::string body = bodyStream.str();
	std::ostringstream response;
	response << "HTTP/1.1 200 OK\r\n"
			 << "Content-Type: " << this->getSameType(fullPath) << "\r\n"
			 << "Content-Length: " << body.size() << "\r\n"
			 << "Connection: close\r\n"
			 << "\r\n"
			 << body;
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
