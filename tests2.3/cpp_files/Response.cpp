#include "Response.hpp"

/////////////////////////////
// CANONICAL ORTHODOX FORM //
/////////////////////////////

// Parameters to constructor :
// 		\ <request> is a reference to the instance of Request that parsed the current request
// 		\ <clientPortaddr> is the portaddr used for the connection ON THE CLIENT SIDE
// 				(ie. the IP address of the client device, and the port on which client receives packets)
// 			-> necessary because CGI specification requires this information to be put into env
Response::Response(Request* request, t_portaddr clientPortaddr)
{
	std::cout << "Normal constructor for Response\n";
	this->_request = request;
	this->_clientPortaddr = clientPortaddr;
	this->_vserv = this->_request->_vserv;
	this->_location = this->_request->_location;
	this->_responseBuffer = NULL;
}

// Minimal constructor to allow an instance of <Client>
// to create a response for malforme requests, with no Request parameter
// (so <status> should have code '400' or '500')
Response::Response(std::string status)
{
	std::cout << "Minimal constructor for Response to request with status " << status << "\n";
	this->_request = NULL;
	this->_location = NULL;
	this->_vserv = NULL;
	this->_responseBuffer = NULL;
	this->makeErrorResponseDefault(status);
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
		eraseLastChar(dirPath);
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
// [cfg feature] define upload path for a certain location
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
// [cfg feature] define 'root'/'alias' path for a certain location of vserver
int	Response::buildFullPathFile(std::string& fullPath)
{
	if (this->_location && !(this->_location->rootPath.empty()))
		fullPath = this->_location->rootPath + this->_request->_filePath;
	else if (this->_location && !(this->_location->aliasPath.empty()))
	{
		if (this->_request->_filePath.size() < this->_location->locationPath.size() - 1)
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
// Returns 0 when all checks succeed, and
// 		\ 1 if a check failed (impossible to build path, unsafe resolution, absent file...)
// 		\ 2 if <stat> indicates that path leads to a directory instead of a regular file
// 			(designed specifically for GET requests, which might want to generate an index file / redirection)
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
			std::cout << "\tis not a regular file ; is a directory : "
				<< S_ISDIR(fileStat.st_mode) << "\n";
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
int	Response::makeRedirResponse(std::string status, std::string newPath)
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
	return (0);
}

//Simple value generator for cookie, dependant on finding blue.txt or green.txt to see the cookie color.
std::string	Response::generateCookie( void )
{
	std::string cookie_value = "no_cookie";

	if (this->_request->_filePath.find("blue.txt") != std::string::npos)
		cookie_value = "blue_cookie";
	else if (this->_request->_filePath.find("green.txt") != std::string::npos)
		cookie_value = "green_cookie";
	return (cookie_value);
}

// Generates the response for a success without body (eg. for successful upload with POST/PUT)
int	Response::makeSuccessResponse(std::string status)
{
	std::stringstream	ss;
	std::string			responseStr;

	std::cout << "[Res::Gen] Generating success response with status " << status << "\n";
	ss << "HTTP/1.1 " << status << "\r\n"
		<< "Content-Length: " << (status.size() + 13) << "\r\n"
		<< "Content-Type: text/html\r\n\r\n"
		<< "Set-Cookie: user-cookie=" << this->generateCookie() << "\r\n"
		<< "<html>" << status << "</html>";
	responseStr = ss.str();
	this->makeResponseFromString(responseStr);
	return (0);
}

// Generates a very simple error page for error <status>
int	Response::makeErrorResponseDefault(std::string status)
{
	std::stringstream		ss;
	std::string				responseStr;

	std::cout << "[Res::Gen] Generating default error page with error " << status << "\n";
	ss << "HTTP/1.1 " << status << "\r\n"
		<< "Content-Length: " << (status.size() + 13) << "\r\n"
		<< "Content-Type: text/html\r\n\r\n"
		<< "<html>" << status << "</html>";
	responseStr = ss.str();
	this->makeResponseFromString(responseStr);
	return (0);
}

// Generates the response for an error : splits the <status> string,
// retrieves the error code in its first token,
// then tries to serve the error page defined by cfg file :
// 		\ virtual server must be defined and have a root path
// 		\ virtual server must have an error page for <code> in its map <errorPages>
// 		\ the error page file must exist and be regular
// If a check fails, instead generates an default simple error page
// /!\ Serving another file when an error occurs cannot produce a loop
// 	   because the only possible error in <makeFileResponse> is 500,
// 	   and no error page can be defined for the range 500-599
// /!\ Relative links to resources in served error page will be broken
// 	   (since the error page served is not at the place targeted by request)
// 			so the HTML error pages should contain a 'base href=[path]' instruction
// 			indicating to the client the base path from which relative links should start
// [cfg feature] configure error page to serve for a specific error code
int	Response::makeErrorResponse(std::string status)
{
	std::vector<std::string>	tokens;
	int							code;
	struct stat					fileStat;
	size_t						fileSize;
	std::string					pagePath;

	std::cout << "[Res::Gen] Handling error : " << status << "\n";
	splitString(status, tokens, " ");
	if (this->_request && this->_vserv && !(this->_vserv->rootPath.empty())
		&& tokens.size() > 0 && !invalidReturnCode(tokens[0]))
	{
		code = static_cast<int>(strtol(tokens[0].c_str(), NULL, 10));
		if (this->_vserv->errorPages.count(code) > 0)
		{
			pagePath = this->_vserv->rootPath + "/" + this->_vserv->errorPages[code];
			if (stat(pagePath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode))
			{
				fileSize = fileStat.st_size;
				return (this->makeFileResponse(pagePath, fileSize, status));
			}
		}
	}
	return (this->makeErrorResponseDefault(status));
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
// [cfg feature] define file extensions to be handled as CGI
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
// for a request not using CGI
int	Response::handleNotCGI(void)
{
	std::cout << "\tRequest without CGI, method : " << this->_request->_method << "\n";
	if (this->_request->_method == "GET")
		return (this->handleGet());
	else if (this->_request->_method == "POST")
		return (this->handlePost());
	else if (this->_request->_method == "DELETE")
		return (this->handleDelete());
	else if (this->_request->_method == "PUT")
		return (this->handlePut());
	else
		return (this->makeErrorResponse("501 Not Implemented"));
}

// Performs checks required by cfg file on request :
// (these checks should succeed, since they are already done very early by <Client>
//  as soon as <Request> instance parsed the headers)
// 		\ (if body exists) body is not too large for the virtual server and for the location
// 		\ method is allowed by targeted location (if there is no location, method must be GET)
// 		\ location is not a redirection (if it is a redirection, generate a redirection response)
// Then calls the correct handling function according to need of CGI
// [cfg feature] : define methods allowed for a location
// 				   define max request body for a vserver/location
// 				   define redirection for a location
int	Response::produceResponse(void)
{
	std::string	newPath;

	std::cout << "[Res::Response] Generating response, virtual server = "
		<< this->_vserv << ", location = " << this->_location << "\n";
	if (this->_request->_hasBody && (this->_request->_bodySize > this->_vserv->maxRequestBodySize
		|| (this->_location && this->_request->_bodySize > this->_location->maxRequestBodySize)))
		return (this->makeErrorResponse("413 Content Too Large"));
	if (!(this->_location) && this->_request->_method != "GET")
		return (this->makeErrorResponse("405 Method Not Allowed"));
	if (this->_location && this->_location->acceptedMethods.count(this->_request->_method) == 0)
		return (this->makeErrorResponse("405 Method Not Allowed"));
	if (this->_location && !(this->_location->redirection.empty()))
	{
		newPath = this->_location->redirection
				+ this->_request->_filePath.substr(this->_location->locationPath.size());
		std::cout << "\tRequest on location with redirection defined, new path : " << newPath << "\n";
		return (this->makeRedirResponse(this->_location->redirectionCode + " Found", newPath));
	}
	if (this->isCGIRequest())
	{
		std::cout << "\tRequest for CGI, extension " << this->_request->_extension << "\n";
		return (this->handleCGI());
	}
	else
		return (this->handleNotCGI());
}
