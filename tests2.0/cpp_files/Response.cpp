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
	std::cout << "Constructor for Response\n";
	this->_request = request;
	this->_clientPortaddr = clientPortaddr;
	this->_vserv = this->identifyVserver(vservers, vservIndexes);
	this->_location = this->identifyLocation();
}

Response::Response(int errcode)
{
	std::cout << "Constructor for Response to request with error " << errcode << "\n";
	this->makeErrorResponse(400);
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

// #f : reorganise with intermediary functions
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
	return (&(vservers.front()));
}

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
		if (this->_request->_path.compare(0, locationPath.size(), locationPath) == 0)
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

// Builds the path to the resource in the server's arborescence
// 		\ if <isUpload> is true and a location is defined, this location may have a specific upload path
// 		\ if a location is defined, this location may have root/alias directive
// 			~ root : rootPath prepended to path in URL
// 			~ alias : aliasPath replaces the part of path in URL corresponding to locationPath
// 		\ if no location undefined or without root/alias, verser may have a root directive
// 		\ in all other cases, the resource can't be found (404)
int	Response::buildFullPath(std::string& fullPath, bool isUpload)
{
	size_t		indLastSlash;
	std::string	filename;

	if (isUpload && this->_location && this->_location->uploadPath != "~")
	{
		indLastSlash = this->_request->_filePath.find_last_of('/');
		if (indLastSlash == std::string::npos)
			return (1);
		else
			filename = this->_request->_filePath.substr(indLastSlash + 1);
		fullPath = this->_location->uploadPath + "/" + filename;
	}
	else if (this->_location && !(this->_location->rootPath.empty()))
		fullPath = this->_location->rootPath + this->_request->_filePath;
	else if (this->_location && !(this->_location->aliasPath.empty()))
	{
		fullPath = this->_location->aliasPath
				 + this->_request->_filePath.substr(this->_location->locationPath.size() - 1);
	}
	else if (!(this->_vserv->rootPath.empty()))
		fullPath = this->_vserv->rootPath + this->_request->_filePath;
	else
		return (1);
	return (0);
}

// Checks that the resolved full path (after executing links/'..')
// begins with the base path used as a result of root/alias directives
// Necessary for security reasons : ensure that the request accesses resources
// only within the part of the arborescence given by the config file
// After checks succeeded, <fullPath> is indeed transformed into resolved full path
int	Response::checkResolvedFullPath(std::string& fullPath, bool isUpload)
{
	char		realBase[PATH_MAX];
	std::string	base;
	std::string	filename;
	size_t		indLastSlash;

	(void)isUpload;
	indLastSlash = fullPath.find_last_of('/');
	base = fullPath.substr(0, indLastSlash);
	filename = fullPath.substr(indLastSlash + 1);
	//std::cout << "base : " << base << ", filename : " << filename << "\n";
	if (!realpath(base.c_str(), realBase))
		return (1);
	if (std::string(realBase).find(base) != 0)
		return (1);
	fullPath = std::string(realBase) + "/" + filename;
	return (0);
}

// Transforms the <_path> parsed by the Request instance
// into the real path to the resource in server's arborescence, by
// 		\ identifying empty <_path> / <_path> pointing to directories
// 		\ building the full path according to 'root'/'alias' directives from config file
// 		\ check if the resolved full path is safe (see <checkResolvedFullPath> description)
// Parameters control additional checks and actions :
// 		\ if <checkFile> is true, check that a regular file exist at the given path
// 		\ if <checkFile> and <checkExec> are true, check that the file is executable
// 		\ if <checkFile> is true and <fileSize> is not NULL, sore the file size in <fileSize>
// 		\ if <isUpload> is true, the path will be built using the location's 'upload' path
int	Response::pathToResource(std::string& fullPath,
	bool checkFile, bool checkExec, size_t* fileSize, bool isUpload)
{
	struct stat	fileStat;

	if (this->_request->_filePath.empty() || this->_request->_filePath[0] != '/')
		this->_request->_filePath = "/";
	std::cout << "\tURL path to resource : " << this->_request->_filePath << "\n";
	if (this->buildFullPath(fullPath, isUpload))
		return (this->makeErrorResponse(404), 1);
	std::cout << "\tfull path built : " << fullPath << "\n";
	if (this->checkResolvedFullPath(fullPath, isUpload))
		return (this->makeErrorResponse(404), 1);
	std::cout << "\tfull path resolved : " << fullPath << "\n";
	if (checkFile)
	{
		if (stat(fullPath.c_str(), &fileStat) != 0 || !S_ISREG(fileStat.st_mode))
			return (this->makeErrorResponse(404), 1);
		if (fileSize)
			*fileSize = fileStat.st_size;
		if (checkExec && !(fileStat.st_mode & S_IXUSR))
			return (this->makeErrorResponse(402), 1); // #f : check error code for "not executable"
	}
	std::cout << "\tfile exists and meets requirements\n";
	return (0);
}

////////////////////////////////
// GENERATE GENERIC RESPONSES //
////////////////////////////////

void	Response::makeSuccessResponse(int infocode)
{
	std::stringstream	ss;
	std::string			responseStr;
	std::string			status;

	std::cout << "[Res::Gen] Generating success response with code " << infocode << "\n";
	if (infocode == 201)
		status = "201 Created";
	else
		status = "200 OK";

	ss << "HTTP/1.1 " << status << "\r\n\r\n";
	responseStr = ss.str();
	this->_responseBuffer = new char[responseStr.size()];
	bzero(this->_responseBuffer, responseStr.size());
	strcpy(this->_responseBuffer, responseStr.c_str());
	this->_responseSize = responseStr.size();
}

// Generates the response for an error according to error type given by errcode
void	Response::makeErrorResponse(int errcode)
{
	std::stringstream	ss;
	std::string			responseStr;

	std::cout << "[Res::Gen] Generating error response with code " << errcode << "\n";
	ss << "HTTP/1.1 " << errcode << " Error\r\n"
		<< "Content-Length: 16\r\n"
		<< "Content-Type: text/html\r\n\r\n"
		<< "<html>" << errcode << "</html>";
	responseStr = ss.str();
	this->_responseBuffer = new char[responseStr.size()];
	bzero(this->_responseBuffer, responseStr.size());
	strcpy(this->_responseBuffer, responseStr.c_str());
	this->_responseSize = responseStr.size();
}

///////////////////////////
// HANDLING GET REQUESTS //
///////////////////////////


int		Response::readFullFileInBuffer(std::string& fullPath, size_t fileSize, char* buffer)
{
	int		fd;
	size_t	ind;
	long	nBytesToRead;
	long	nBytesRead;

	fd = open(fullPath.c_str(), O_RDONLY);
	if (fd < 0)
		return (1);
	ind = 0;
	while (ind < fileSize)
	{
		nBytesToRead = std::min(READFILE_BUFFER_SIZE, fileSize - ind);
		nBytesRead = read(fd, buffer + ind, nBytesToRead);
		if (nBytesRead < 0)
			return (close(fd), 1);
		ind += nBytesRead;
	}
	close(fd);
	return (0);
}

// Opens the requested resource, generates the response headers,
// then appends the resource's content to the response as its body
int	Response::makeFileResponse(std::string& fullPath, size_t fileSize)
{
	std::stringstream	headersStream;
	std::string			headersString;
	size_t				totalPayloadSize;

	// Create HTTP format headers for response
	headersStream << "HTTP/1.1 200 OK\r\n"
			 << "Content-Type: " << this->getResponseBodyType() << "\r\n"
			 << "Content-Length: " << fileSize << "\r\n"
			 << "\r\n";
	headersString = headersStream.str();
	totalPayloadSize = headersString.size() + fileSize;
	std::cout << "[Res::fileResponse] headers of size " << headersString.size() << " + file of size "
		<< fileSize << " = payload of size " << totalPayloadSize << " to be allocated\n";
	// Allocate response buffer with enough space for whole file content
	this->_responseBuffer = new char[totalPayloadSize];
	bzero(this->_responseBuffer, totalPayloadSize);
	strcpy(this->_responseBuffer, headersString.c_str());
	if (this->readFullFileInBuffer(fullPath, fileSize, this->_responseBuffer + headersString.size()))
	{
		delete[] this->_responseBuffer; this->_responseBuffer = NULL;
		return (makeErrorResponse(500), 0);
	}
	this->_responseSize = totalPayloadSize;
	return (0);
}

// Returns a string containing the exact MIME type of the returned resource
// according to its file extension
std::string	Response::getResponseBodyType()
{
	if (this->_request->_extension == ".html")
		return ("text/html");
	if (this->_request->_extension == ".jpg" || this->_request->_extension == ".jpeg")
		return ("image/jpeg");
	if (this->_request->_extension == ".png")
		return ("image/png");
	if (this->_request->_extension == ".css")
		return ("text/css");
	if (this->_request->_extension == ".js")
		return ("application/javascript");
	return ("application/octet-stream");
}

// Handles GET requests with no CGI -> the request asks for the content of a specific file 
// Checks that the requested resource exists before generating the response containing its content
int	Response::handleGet()
{
	std::string	fullPath;
	size_t		fileSize;

	if (this->pathToResource(fullPath, 1, 0, &fileSize, 0))
		return (0); // #f : check if <makeErrorResponse> should be at this level
	if (fileSize > FILEDOWNLOAD_MAXSIZE)
		return (this->makeErrorResponse(413), 0);
	return (this->makeFileResponse(fullPath, fileSize));
}

//////////////////////////
// HANDLE POST REQUESTS //
//////////////////////////

int	Response::writeFullBufferInFile(std::string& fullPath, size_t bufferSize, char *buffer)
{
	int		fd;
	size_t	ind;
	long	nBytesToWrite;
	long	nBytesWritten;

	fd = open(fullPath.c_str(), O_WRONLY | O_CREAT, 0644);
	if (fd < 0)
		return (1);
	ind = 0;
	while (ind < bufferSize)
	{
		nBytesToWrite = std::min(WRITEFILE_BUFFER_SIZE, bufferSize - ind);
		nBytesWritten = write(fd, buffer + ind, nBytesToWrite);
		if (nBytesWritten < 0)
			return (close(fd), 1);
		ind += nBytesWritten;
	}
	close(fd);
	return (0);
}

// Handles POST requests without CGI -> the request contains the content of a file
// that must be stored on the server side at the location given by the path
// Checks that 
int	Response::handlePost()
{
	std::string	fullPath;
	bool		fileExists;

	std::cout << "\tchecks on POST request for uploading a file\n";
	if (!(this->_location) || this->_location->uploadPath.empty())
		return (this->makeErrorResponse(415), 0); // #f : check error code for 'upload forbidden'
	if (this->pathToResource(fullPath, 0, 0, NULL, 1))
		return (0); // #f : check if <makeErrorResponse> should be at this level
	if (this->_request->_bodySize > FILEUPLOAD_MAXSIZE)
		return (this->makeErrorResponse(413), 0);
	fileExists = (access(fullPath.c_str(), F_OK) == 0);
	std::cout << "\tchecks succeeded ; file already existed : " << fileExists << "\n";
	if (this->writeFullBufferInFile(fullPath, this->_request->_bodySize, this->_request->_body))
		return (this->makeErrorResponse(500), 0);
	std::cout << "\tfile upload finished\n";
	if (fileExists)
		this->makeSuccessResponse(200);
	else
		this->makeSuccessResponse(201);
	return (0);
}

int	Response::handleDelete()
{
	std::string	fullPath;
	bool		fileExists;

	std::cout << "\tchecks on DELETE\n";
	if (this->pathToResource(fullPath, 1, 0, NULL, 0))
		return (0); // #f : check if <makeErrorResponse> should be at this level
	fileExists = (access(fullPath.c_str(), F_OK) == 0);
	std::cout << "\tchecks ended ; file exists : " << fileExists << "\n";
	if (!fileExists)
		return (this->makeErrorResponse(500), 0);
	if (remove(fullPath.c_str()))
		return (this->makeErrorResponse(500), 0);
	std::cout << "\tfile successfully deleted\n";
	this->makeSuccessResponse(200);
	return (0);
}


///////////////////////
// GENERATE RESPONSE //
///////////////////////

// Check if the request demands the execution of a CGI script/program, with 2 conditions
// 		\ request method must be GET or POST
// 		\ request must be directed to a location, and the requested file
// 		  must have an extension belonging to the CGI extensions list of that location
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
// Displays the headers AND THE BODY without any checks
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
// #f : check that request is allowed at the given location
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
			this->makeErrorResponse(501);
			return (0);
		}
	}
}
