#include "Response.hpp"

/////////////////////////////
// CANONICAL ORTHODOX FORM //
/////////////////////////////

// Parameters to constructor :
// 		\ <request> is a reference to the instance of Request that parsed the current request
// 		\ <clientPortaddr> is the portaddr used for the connection ON THE CLIENT SIDE
// 				(ie. the IP address of the client device, and the port on which client receives packets)
// 			-> necessary because CGI specification requires this information to be put into env
// 		\ <vservIndexes> is (a reference to) the list of vservers listening
// 		  on the server portaddr on which the connection is occurring (<=> on which the request was received)
// 			-> necessary to identify which vserver should respond to the request
// 		\ <vservers> is (a reference to) a list of structures representing the virtual servers
// 			(the indexes in <mainSocket.vservIndexes> are positions in this list)
Response::Response(Request* request, t_portaddr clientPortaddr,
	std::vector<t_vserver>& vservers, std::set<int>& vservIndexes)
{
	std::cout << "Normal constructor for Response\n";
	this->_request = request;
	this->_clientPortaddr = clientPortaddr;
	this->_vserv = this->identifyVserver(vservers, vservIndexes);
	this->_location = this->identifyLocation();
}

// Minimal constructor to allow an instance of <Client>
// to create a response for malforme requests, with no Request parameter
// (so <status> should always have code '400' or '500')
Response::Response(std::string status)
{
	std::cout << "Minimal constructor for Response to request with status " << status << "\n";
	this->makeErrorResponse(status);
}

Response::~Response()
{
	std::cout << "Destructor for Response\n";
	if (this->_responseBuffer)
		delete[] this->_responseBuffer;
}

///////////////
// ACCESSORS //
///////////////

char*	Response::getResponseBuffer()
{
	return (this->_responseBuffer);
}

unsigned long	Response::getResponseSize()
{
	return (this->_responseSize);
}

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
t_vserver*	Response::identifyVserver(std::vector<t_vserver>& vservers, std::set<int>& vservIndexes)
{	
	std::set<int>::iterator					vservInd;
	t_vserver*								vserv;
	std::set<t_portaddr >::iterator			vservPortaddr;
	std::set<std::string>::iterator			vservName;
	bool									portaddrMatch;
	bool									nameMatch;

	// try matching host description in request with virtual servers
	for (vservInd = vservIndexes.begin(); vservInd != vservIndexes.end(); vservInd++)
	{
		vserv = &(vservers[*vservInd]);
		// Check if 1 of virtual server's portaddrs matches host portaddr
		portaddrMatch = 0;
		for (vservPortaddr = vserv->portaddrs.begin();
			vservPortaddr != vserv->portaddrs.end(); vservPortaddr++)
		{
			if (portaddrContains(*vservPortaddr, this->_request->_hostPortaddr))
			{
				portaddrMatch = 1;
				break;
			}
		}
		// if no match, it cannot be this virtual server
		if (!portaddrMatch)
			continue;
		// if portaddr match and no hostName in request, then portaddr match is enough
		if (this->_request->_hostName.size() == 0)
			return (vserv);
		// if portaddr match and hostName defined,
		// then check if hostName matches 1 of the virtual server's name
		nameMatch = 0;
		for (vservName = vserv->serverNames.begin();
			vservName != vserv->serverNames.end(); vservName++)
		{
			if (this->_request->_hostName == *vservName)
			{
				nameMatch = 1;
				break;
			}
		}
		if (nameMatch)
			return (vserv);
	}
	// when no virtual server matches, return the first server in the list
	std::cout << "[Res::idVserv] no match found -> returning first possible vserver\n";
	return (&(vservers[*(vservIndexes.begin())]));
}

// Returns a pointer to the location, in the current virtual server,
// targeted by the request in <this._request>
// Loops through the virtual server's locations searching for a match
// between the location's path and the path given in request ;
// 		if multiple locations match the one with the longest location path prevails
// If no match found, returns NULL (a request can target no location)
// Example : if request path is '/a/b/c.txt', locations are '/a/' '/a/b/' '/a/b/c/',
// 		then location '/a/b/' will be chosen (longest match)
t_location*	Response::identifyLocation()
{
	t_location*		bestMatch;
	size_t			bestLength = 0;
	unsigned int	ind;
	std::string		locationPath;

	bestMatch = NULL;
	for (ind = 0; ind < this->_vserv->locations.size(); ind++)
	{
		locationPath = this->_vserv->locations[ind].locationPath;
		if (this->_request->_filePath.compare(0, locationPath.size(), locationPath) == 0)
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

///////////////////////
// PATH TO RESOURCES //
///////////////////////

// Check that the resolved directory path (after executing links/'..')
// 		contains the original directory path at the beginning
// Necessary for security reasons : ensure that the request accesses resources
// only within the part of the arborescence given by the config file
// After checks succeeded, <dirPath> is indeed transformed into resolved directory path
// 		including the trailing '/'
int	Response::checkResolvedDirPath(std::string& dirPath)
{
	char		realDirPath[PATH_MAX];

	if (*(dirPath.end() - 1) == '/')
		dirPath.erase(dirPath.end() - 1);
	if (!realpath(dirPath.c_str(), realDirPath))
		return (1);
	if (std::string(realDirPath).find(dirPath) != 0)
		return (1);
	dirPath = std::string(realDirPath) + "/";
	return (0);
}

// For POST/PUT requests, builds the path where the uploaded file should be stored,
// using path additions defined in cfg file
// (checks on request ensures that location is defined,
//  checks on cfg file ensures that this location has an upload path or a root/alias path)
// 		\ this location may have an upload path, if so it replaces the <dirPath>
// 		\ if uploadPath undefined this location has a root or upload path
// The path is built from parameter <dirPath> (the directory part of request's <_filePath>)
// 		and stored into that same parameter
int	Response::buildFullPathUpload(std::string& dirPath)
{
	if (!(this->_location) || this->_location->uploadPath.empty())
		return (1);
	if (this->_location->uploadPath != "~")
		dirPath = this->_location->uploadPath + "/";
	else if (!(this->_location->rootPath.empty()))
		dirPath = this->_location->rootPath + dirPath;
	else if (!(this->_location->aliasPath.empty()))
	{
		if (dirPath.size() < this->_location->locationPath.size())
			return (1);
		dirPath = this->_location->aliasPath
				+ dirPath.substr(this->_location->locationPath.size() - 1);
	}
	else
		return (1);
	return (0);
}

// Transforms the <_filePath> parsed by the POST/PUT Request instance
// into the real path to the place where the uploaded file should be stored
// Function can be called with <_filePath> leading to a file or to a directory,
// 		in the latter case <isDir> must be set so that <filename> remains an empty string
// Performs steps :
// 		\ check for empty path / path not beginning with '/'
// 		\ build directory path according to 'upload'/'root'/'alias' directives from config file
// 		\ check if resolved directory path is safe (see <checkResolvedFullPath> description)
// 		\ check if directory path leads indeed to a directory with <stat>
int	Response::pathToUpload(std::string& fullPath, bool isDir)
{
	std::string		dirPath;
	std::string		filename = "";
	struct stat		dirStat;

	if (this->_request->_filePath.empty() || this->_request->_filePath[0] != '/')
		return (1);
	if (isDir)
		dirPath = fullPath;
	else if (divideFilePath(this->_request->_filePath, dirPath, filename))
		return (1);
	std::cout << "\tcomputing path to upload : request path has directory path "
		<< dirPath << " and filename " << filename << "\n";
	if (this->buildFullPathUpload(dirPath))
		return (1);
	std::cout << "\tdirectory path after build : " << dirPath << "\n";
	if (this->checkResolvedDirPath(dirPath))
		return (1);
	std::cout << "\tdirectory path after resolution : " << dirPath << "\n";
	if (stat(dirPath.c_str(), &dirStat) != 0)
		return (1);
	if (!S_ISDIR(dirStat.st_mode))
	{
		std::cout << "\tdirectory path does not lead to a directory\n";
		return (1);
	}
	fullPath = dirPath + filename;
	return (0);
}

// Builds the path to the resource in the server's arborescence
// using path additions defined in cfg file
// 		\ if a location is defined, this location may have root/alias directive
// 			~ root : rootPath prepended to path in URL
// 			~ alias : aliasPath replaces the part of path in URL corresponding to locationPath
// 		\ if no location undefined or without root/alias, vserver may have a root directive
// 		\ in all other cases (and in the case of an 'alias' that can't be applied),
// 			the resource can't be found, so the function returns 1 resulting in a '404' error
// The path is built from attribute <_filePath> of the request,
// 		and stored into parameter <fullPath> (which is a reference)
int	Response::buildFullPathFile(std::string& fullPath)
{
	if (this->_location && !(this->_location->rootPath.empty()))
		fullPath = this->_location->rootPath + this->_request->_filePath;
	else if (this->_location && !(this->_location->aliasPath.empty()))
	{
		if (this->_request->_filePath.size() < this->_location->locationPath.size())
			return (1);
		fullPath = this->_location->aliasPath
				 + this->_request->_filePath.substr(this->_location->locationPath.size() - 1);
	}
	else if (!(this->_vserv->rootPath.empty()))
		fullPath = this->_vserv->rootPath + this->_request->_filePath;
	else
		return (1);
	return (0);
}

// Transforms the <_filePath> parsed by the Request instance
// into the real path to the resource in server's arborescence
// 		(function should only be called when <_filePath> does not end with '/')
// Performs steps :
// 		\ checking for empty path / path not beginning with '/'
// 		\ building the full path according to 'root'/'alias' directives from config file
// 		\ check if the resolved full path is safe (see <checkResolvedFullPath> description)
// Parameters control additional checks and actions :
// 		\ if <checkFile> is true, check that a regular file exist at the given path
// 		\ if <checkFile> and <checkExec> are true, check that the file is executable
// 		\ if <checkFile> is true and <fileSize> is not NULL, stores the file size in <fileSize>
// Returns 0 normally, 1 if a check failed (impossible to build path, unsafe resolution, absent file)
// 		and 2 specifically for GET requests if <stat> indicates that path leads to a directory
// 		instead of a regular file (so that a redirection response to the directory can be generated)
int	Response::pathToFile(std::string& fullPath,
	bool checkFile, bool checkExec, size_t* fileSize)
{
	struct stat	fileStat;
	std::string filename;
	std::string dirPath;

	if (this->_request->_filePath.empty() || this->_request->_filePath[0] != '/')
		return (1);
	std::cout << "\tURL path to resource : " << this->_request->_filePath << "\n";
	if (this->buildFullPathFile(fullPath))
		return (1);
	std::cout << "\tfull path built : " << fullPath << "\n";
	if (divideFilePath(fullPath, dirPath, filename))
		return (1);
	if (this->checkResolvedDirPath(dirPath))
		return (1);
	fullPath = dirPath + filename;
	std::cout << "\tfull path resolved : " << fullPath << "\n";
	if (checkFile)
	{
		if (stat(fullPath.c_str(), &fileStat) != 0)
			return (1);
		if (!S_ISREG(fileStat.st_mode))
		{
			std::cout << "\tis not a regular file ; is a directory : " << S_ISDIR(fileStat.st_mode) << "\n";
			if (S_ISDIR(fileStat.st_mode))
				return (2);
			else
				return (1);
		}
		if (fileSize)
			*fileSize = fileStat.st_size;
		if (checkExec && !(fileStat.st_mode & S_IXUSR))
			return (1);
	}
	std::cout << "\tall file checks succeeded\n";
	return (0);
}

///////////////////////
// GENERIC RESPONSES //
///////////////////////

// Allocates and fills the response buffer for a response without body,
// contained entirely in the string <responseStr> given as parameter
void	Response::makeResponseFromString(std::string& responseStr)
{
	this->_responseBuffer = new char[responseStr.size() + 1];
	bzero(this->_responseBuffer, responseStr.size() + 1);
	strcpy(this->_responseBuffer, responseStr.c_str());
	this->_responseSize = responseStr.size();
}

// Generates a redirect response according to parameters
// 		\ if <newPath> is defined, uses this path as the redirected location
// 		\ if <newPath> is empty (case of a GET request with path to a directory but no trailing '/'),
// 			uses the <_filePath> of the request with an added '/'
void	Response::makeRedirResponse(std::string status, std::string newPath)
{
	std::stringstream	ss;
	std::string			responseStr;

	std::cout << "[Res::Gen] Generating redirect response with status " << status << "\n";
	if (newPath.empty())
		newPath = this->_request->_filePath + "/";
	ss << "HTTP/1.1 " << status << "\r\n"
		<< "Content-Length: 0" << "\r\n"
		<< "Location: " << newPath << "\r\n\r\n";
	responseStr = ss.str();
	this->makeResponseFromString(responseStr);
}

// Generates the response for a success without body (eg. for successful upload with POST/PUT)
void	Response::makeSuccessResponse(std::string status)
{
	std::stringstream	ss;
	std::string			responseStr;

	std::cout << "[Res::Gen] Generating success response with status " << status << "\n";
	ss << "HTTP/1.1 " << status << "\r\n"
		<< "Content-Length: 0" << "\r\n\r\n";
	responseStr = ss.str();
	this->makeResponseFromString(responseStr);
}

// Generates the response for an error according to error type given by errcode
void	Response::makeErrorResponse(std::string status)
{
	std::stringstream	ss;
	std::string			responseStr;

	std::cout << "[Res::Gen] Generating error response status " << status << "\n";
	ss << "HTTP/1.1 " << status << "\r\n"
		<< "Content-Length: " << (status.size() + 12) << "\r\n"
		<< "Content-Type: text/html\r\n\r\n"
		<< "<html>" << status << "</html>";
	responseStr = ss.str();
	this->makeResponseFromString(responseStr);
}

///////////////////////
// GENERATE RESPONSE //
///////////////////////

// Check if the request demands the execution of a CGI script/program, with 2 conditions
// 		\ request method must be GET or POST
// 		\ request must target a location and request a file with an extension
// 		\ request extension must belong to the CGI extensions list of the location
// 			(extensions will mostly be '.php' or '.py',
// 			 but any extension can be defined as CGI extension in the config file)
bool	Response::isCGIRequest()
{
	unsigned int	ind;
	
	if (!(this->_location) || this->_location->cgiExtensions.size() == 0
		|| (this->_request->_method != "GET" && this->_request->_method != "POST"))
		return (0);
	for (ind = 0; ind < this->_location->cgiExtensions.size(); ind++)
	{
		if (this->_request->_extension == this->_location->cgiExtensions[ind])
			return (1);
	}
	return (0);
}

// Displays the current state of the response buffer
// /!\ Displays the headers AND THE BODY without any checks
// 		-> do not use when binary data is transmitted
void	Response::logResponseBuffer()
{
	size_t	ind;

	std::cout << "[Res::Response] Generated response in buffer :\n";
	std::cout << "\033[32m< HTTP\n";
	for (ind = 0; ind < this->_responseSize; ind++)
		std::cout << this->_responseBuffer[ind];
	std::cout << "\n>\033[0m\n";
}

// Calls the correct handling functions to generate the HTTP response
// according to the request's method and use of CGI
// #f : check POST request with no body and not a CGI 
// 		check POST request targeting existing static file
// #f : check that request is allowed at the given location (else, 405 error code)
// #f : check existence of content length and content type for requests with a body
// #f : check request ending with '/'
int	Response::produceResponse(void)
{
	std::cout << "[Res::Response] Generating response, virtual server = "
		<< this->_vserv << ", location = " << this->_location << "\n";
	if (this->isCGIRequest())
	{
		std::cout << "\tRequest for CGI, extension " << this->_request->_extension << "\n";
		return (this->handleCGI());
	}
	else
	{
		std::cout << "\tRequest without CGI, method : " << this->_request->_method << "\n";
		if (this->_request->_method == "GET")
			return (this->handleGet());
		else if (this->_request->_method == "POST")
			return (this->handlePost());
		else if (this->_request->_method == "DELETE")
			return (this->handleDelete());
		else
		{
			this->makeErrorResponse("501 Not Implemented");
			return (0);
		}
	}
}
