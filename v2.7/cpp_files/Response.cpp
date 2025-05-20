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

int	Response::getCGIChildFd()
{
	return (this->_cgiReadFd);
}

///////////////////////
// PATH TO RESOURCES //
///////////////////////

// This section is the ugliest of the project, and needs an overhaul

// Check that the resolved directory path (after executing links/'..')
// 		contains the original directory path at the beginning
// Necessary for security reasons : ensure that the request accesses resources
// only within the part of the arborescence given by the config file
// After checks succeeded, <dirPath> is indeed transformed into resolved directory path
// 		including the trailing '/'
int	Response::checkResolvedDirPath(std::string& dirPath)
{
	char		realDirPath[PATH_MAX];

	if (dirPath == "/")
		return (0);
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
// 		\ checks : location must be defined, and have an upload path
// 			(this should always be true because it is checked earlier)
// 			(a non-empty upload path with value "~" means 'here', 'here' being defined with 'alias'/'root')
// 		\ this location may have an upload path, if so it replaces the <dirPath>
// 		\ if uploadPath non-empty but undefined (value "~"), this location must have a root or alias path
// The path is built from parameter <dirPath> (the directory part of request's <_filePath>)
// 		and stored into that same parameter
// [cfg feature] define upload path for a certain location
int	Response::buildFullPathUpload(std::string& dirPath)
{
	std::string		origPath;

	if (!(this->_location) || this->_location->uploadPath.empty())
		return (1);
	if (this->_location->uploadPath != "~")
	{
		dirPath = this->_location->uploadPath + "/";
		return (0);
	}
	origPath = dirPath;
	return (this->buildFullPathFile(dirPath, origPath));
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
		dirPath = this->_request->_filePath;
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
// The path is built from attribute <origPath> (usually derived from <Request.filePath>) of the request,
// 		and stored into parameter <fullPath> (which is a reference)
// [cfg feature] define 'root'/'alias' path for a certain location of vserver
int	Response::buildFullPathFile(std::string& fullPath, std::string& origPath)
{
	if (this->_location && !(this->_location->rootPath.empty()))
	{
		if (this->_location->rootPath == "/")
			fullPath = origPath;
		else
			fullPath = this->_location->rootPath + origPath;
	}
	else if (this->_location && !(this->_location->aliasPath.empty()))
	{
		if (origPath.size() < this->_location->locationPath.size() - 1)
			return (1);
		if (this->_location->aliasPath == "/" && origPath.size() > this->_location->locationPath.size() - 1)
			fullPath = origPath.substr(this->_location->locationPath.size() - 1);
		else
			fullPath = this->_location->aliasPath
					 + origPath.substr(this->_location->locationPath.size() - 1);
	}
	else if (!(this->_vserv->rootPath.empty()))
	{
		if (this->_location->rootPath == "/")
			fullPath = origPath;
		else
			fullPath = this->_vserv->rootPath + origPath;
	}
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
// 			(this was used for the previous implementation of CGI, but is now useless)
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
	if (this->buildFullPathFile(fullPath, this->_request->_filePath))
		return (1);
	std::cout << "\tpath built : " << fullPath << "\n";
	if (divideFilePath(fullPath, dirPath, filename))
		return (1);
	std::cout << "\tpath divided : " << fullPath << "\n";
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
	if (this->_responseBuffer)
	{
		delete[] this->_responseBuffer;
		this->_responseBuffer = NULL;
	}
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

// Generates the response for a success without body (eg. for successful upload with POST/PUT)
// For requests done through a browser, the browser will display a blank page
int	Response::makeSuccessResponse(std::string status)
{
	std::stringstream	ss;
	std::string			responseStr;

	std::cout << "[Res::Gen] Generating success response with status " << status << "\n";
	ss << "HTTP/1.1 " << status << "\r\n";
	ss << "Content-Length: 0\r\n\r\n";
	responseStr = ss.str();
	this->makeResponseFromString(responseStr);
	return (0);
}

// Generates a very simple error page for error <status>
int	Response::makeErrorResponseDefault(std::string status)
{
	std::stringstream		ssa;
	std::stringstream		ssb;
	std::string				responseStr;
	std::string				bodyStr;

	std::cout << "[Res::Gen] Generating default error page with error " << status << "\n";
	ssa << "<html><head><title>Error !</title></file></head>"
		<< "<body><p>Your request generated an error : " << status << "</p></body></html>";
	bodyStr = ssa.str();
	ssb << "HTTP/1.1 " << status << "\r\n"
		<< "Content-Length: " << bodyStr.size() << "\r\n"
		<< "Content-Type: text/html\r\n\r\n"
		<< bodyStr;
	responseStr = ssb.str();
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
// 		\ request extension must belong to the CGI extensions map of the location
// 			(for Webserv, extensions will mostly be '.php' or '.py')
// [cfg feature] define file extensions to be handled as CGI with an interpreter
bool	Response::isCGIRequest()
{
	std::map<std::string, std::string>::iterator	it;

	if (!(this->_location) || this->_location->cgiInterpreted.size() == 0
		|| (this->_request->_method != "GET" && this->_request->_method != "POST"))
		return (0);
	if (this->_location->cgiInterpreted.count(this->_request->_extension) > 0)
		return (1);
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

// Determines the redirected path to give to the client in the "Location" header :
// it is the same path as in the request, except the part that matched the location path
// is replaced by the part given in <this._location.redirection> (which must end with '/')
int	Response::redirectPath()
{
	std::string	newPath;

	if (this->_request->_filePath.size() <= this->_location->locationPath.size())
		newPath = this->_location->redirection;
	else
	{
		newPath = this->_location->redirection
				+ this->_request->_filePath.substr(this->_location->locationPath.size());
	}
	std::cout << "\tRequest on location with redirection defined, new path : " << newPath << "\n";
	return (this->makeRedirResponse(this->_location->redirectionCode + " Found", newPath));
}

// Calls the correct handling functions to generate the HTTP response
// for a request not using CGI
int	Response::handleNotCGI()
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
// Has 4 potential return values :
// 		\ 0 : response created successfully
// 		\ 1 : encountered fatal error (current implementation never returns this)
// 		\ 2 : redirection (<path> of <Request> instance was modified, need to reprocess all)
// 		\ 3 : CGI script was launched to generate response, now must wait for it to terminate
// [cfg feature] : define methods allowed for a location
// 				   define max request body for a vserver/location
// 				   define redirection for a location
int	Response::produceResponse()
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
		return (this->redirectPath());
	if (this->isCGIRequest())
	{
		std::cout << "\tRequest for CGI, extension " << this->_request->_extension << "\n";
		return (this->handleCGI());
	}
	else
		return (this->handleNotCGI());
}
