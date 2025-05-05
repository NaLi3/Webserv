#include "Response.hpp"

Response::Response( Request& request, t_mainSocket& mainSocket, std::vector<t_vserver>& vservers,
	std::set<int>& vservIndexes) : _request(request), _mainSocket(mainSocket)
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

t_vserver*	Response::identifyVserver(std::vector<t_vserver>& vservers, std::set<int>& vservIndexes)
{
	std::string						host = (*((this->_request.getHeaders()).find(std::string("Host")))).second;
	std::string						hostName = "";
	std::string						hostPort = "";
	std::pair<uint32_t, uint16_t>	hostPortaddr;
	unsigned long					posSeparator;
	in_addr							addr;

	posSeparator = host.find(':');
	if (posSeparator == std::string::npos)
		hostName = host;
	else
	{
		hostName = host.substr(0, posSeparator);
		hostPort = host.substr(posSeparator + 1);
	}
	if ((hostName.compare("localhost") == 0 && inet_aton("127.0.0.1", &addr))
		|| inet_aton(hostName.c_str(), &addr))
	{
		hostPortaddr.first = ntohl(addr.s_addr);
		hostName = "";
	}
	else
		hostPortaddr.first = this->_mainSocket.portaddr.first;
	if (hostPort.size() > 0)
		hostPortaddr.second = static_cast<uint16_t>(strtol(hostPort.c_str(), NULL, 10));
	std::cout << "Host string in request : " << host << " -> hostName " << hostName
		<< " + hostPort " << hostPort << " -> portaddr " << hostPortaddr << "\n";
	
	std::set<int>::iterator								vservInd;
	std::set<std::pair<uint32_t, uint16_t> >::iterator	vservPortaddr;
	std::set<std::string>::iterator						vservName;
	bool												portaddrMatch;
	bool												nameMatch;
	
	for (vservInd = vservIndexes.begin(); vservInd != vservIndexes.end(); vservInd++)
	{
		portaddrMatch = 0;
		for (vservPortaddr = vservers[*vservInd].portaddrs.begin();
			vservPortaddr != vservers[*vservInd].portaddrs.end(); vservPortaddr++)
		{
			if (portaddrContains(*vservPortaddr, hostPortaddr))
			{
				portaddrMatch = 1;
				break;
			}
		}
		if (!portaddrMatch)
			continue;
		if (hostName.size() == 0)
			return (&(vservers[*vservInd]));
		nameMatch = 0;
		for (vservName = vservers[*vservInd].serverNames.begin();
			vservName != vservers[*vservInd].serverNames.end(); vservName++)
		{
			if (hostName.compare(*vservName) == 0)
			{
				nameMatch = 1;
				break;
			}
		}
		if (nameMatch)
			return (&(vservers[*vservInd]));
	}
	return (&(*(vservers.begin())));
}

t_location*	Response::identifyLocation()
{
	unsigned int	ind;
	std::string		locationPath;

	for (ind = 0; ind < this->_vserv->locations.size(); ind++)
	{
		locationPath = this->_vserv->locations[ind].locationPath;
		if (this->_request.getPath().compare(0, locationPath.size(), locationPath) == 0)
			return (&(this->_vserv->locations[ind]));
	}
	return (NULL);
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
		std::cout << "Request without CGI\n"; // #f : join with handling of non-CGI request
		this->_responseBuffer = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nBASE";
		return (0);
	}
}