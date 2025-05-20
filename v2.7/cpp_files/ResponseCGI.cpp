#include "../hpp_files/webserv.hpp"

//////////////////////////////////////
// HANDLING CGI - GENERATE RESPONSE //
//////////////////////////////////////

// Generates the HTTP response, after CGI output is processed, for case : client redirection
// 		(<=> indicate the client to send a new request, with the path given in "Location" instead)
// Checks that CGI output contains only the "Location" header and that status code is 302, as required by RFC
// The only header sent is "Location" (no "Set-Cookie" headers)
int		Response::generateCGIResponseClientRedirect(std::multimap<std::string, std::string>& cgiHeaders,
						std::string& statusCode, std::string& statusDesc)
{
	std::stringstream	ss;
	std::string			responseStr;

	std::cout << "\t-> generating response for client redirect, without document\n";
	if (cgiHeaders.size() > 1 || this->_cgiOutputBody.size() > 0)
		return (this->makeErrorResponse("500 Internal Server Error (CGI output credir with non-Location headers)"));
	if (statusCode.empty())
	{
		statusCode = "302";
		statusDesc = "Found";
	}
	else if (statusCode != "302")
		return (this->makeErrorResponse("500 Internal Server Error (CGI output credir without code 302)"));

	this->makeRedirResponse(statusCode + " " + statusDesc, (*(cgiHeaders.find("location"))).second);
	return (0);
}

// Handles, after CGI output is processed, the case : local redirection
// Although the function is named <generateCGIResponse...> for coherence,
// it does not create an HTTP response in <_responseBuffer>, instead
// 		\ it checks that CGI output contains only the "Location" header as required by RFC
// 		\ updates the path in the instance of <Request> to be the path delivered by the CGI
// 		\ returns special return value 2 to indicate the calling instance of <Client>
// 			that the request must be reprocessed (with a new <Response> instance) because of redirection
// All headers set in the CGI output are forgotten, including "Set-Cookie" headers
// (response will be exactly the response to a simple request for the resource identified by the new path)
int		Response::generateCGIResponseLocalRedirect(std::multimap<std::string, std::string>& cgiHeaders,
						std::string& statusCode, std::string& statusDesc)
{
	std::string		responseStr;
	std::string&	newPath = (*(cgiHeaders.find("location"))).second;

	(void)statusCode; (void)statusDesc;
	std::cout << "\t-> generating response for local redirect to " << newPath << "\n";
	if (cgiHeaders.size() > 1 || this->_cgiOutputBody.size() > 0)
		return (this->makeErrorResponse("500 Internal Server Error (CGI output lredir with non-Location headers)"));
	if (this->_request->redirectPath(newPath, ""))
		return (this->makeErrorResponse("500 Internal Server Error (CGI output lredir with invalid new path)"));
	return (2);
}

// Checks the coherence of CGI output headers
// ("Content-Length" must be absent, "Content-Type" present when there is a body)
int Response::checkCGIOutputBody(std::multimap<std::string, std::string>& cgiHeaders)
{
	if (cgiHeaders.count("content-length") > 0)
		return (logError("\tError: CGI defined header Content-Length", 0));
	if (this->_cgiOutputBody.size() > 0 && cgiHeaders.count("content-type") == 0)
		return (logError("\tError: CGI did not define header Content-Type", 0));
	return (0);
}

// For CGI requests resulting in a document response with/without redirect,
// allocates the response buffer, then concatenates in it :
// 	  the status line + all header lines + the CGI output body (if there is one)
// (the CGI output body content was stored as <std::vector> of characters,
// 		so its bytes are added one by one to the response buffer)
// All headers set in the CGI output are transferred in the response, including "Set-Cookie" headers
// 		(RFC specifies that there may be multiple "Set-Cookie" headers
int	Response::generateCGIResponseWithDoc(std::multimap<std::string, std::string>& cgiHeaders,
						std::string& statusCode, std::string& statusDesc)
{
	std::stringstream								headersStream;
	std::string										headersString;
	size_t											totalSize;
	size_t											ind;											
	std::string										responseHeaders;
	std::map<std::string, std::string>::iterator	headersIt;

	headersStream << "HTTP/1.1 " << statusCode << " " << statusDesc << "\r\n"
		<< "Content-Length: " << itostr(this->_cgiOutputBody.size()) << "\r\n";
	for (headersIt = cgiHeaders.begin(); headersIt != cgiHeaders.end(); headersIt++)
		headersStream << (*headersIt).first << ": " << (*headersIt).second << "\r\n";
	headersStream << "\r\n";
	headersString = headersStream.str();
	totalSize = headersString.size() + this->_cgiOutputBody.size();
	std::cout << "[Res::fileResponse] headers of size " << headersString.size() << " + CGI output body of size "
		<< this->_cgiOutputBody.size() << " = payload of size " << totalSize << " to be allocated\n";
	// Allocate response buffer with enough space for whole CGI output body
	this->_responseBuffer = new char[totalSize + 1];
	bzero(this->_responseBuffer, totalSize);
	strcpy(this->_responseBuffer, headersString.c_str());
	// Copy CGI output body into response buffer (following headers part)
	for (ind = 0; ind < this->_cgiOutputBody.size(); ind++)
		this->_responseBuffer[headersString.size() + ind] = this->_cgiOutputBody[ind];
	this->_responseSize = totalSize;
	return (0);
}

// Generates the HTTP response, after CGI output is processed, for case : client redirection with document
// Checks that headers are coherent and that the status code is 302 as required by RFC,
// 		then calls <generateCGIResponseWithDoc> to copy CGI output body inside response buffer
int		Response::generateCGIResponseClientRedirectDoc(std::multimap<std::string, std::string>& cgiHeaders,
						std::string& statusCode, std::string& statusDesc)
{
	std::cout << "\t-> generating response for client redirect, with document\n";
	if (this->checkCGIOutputBody(cgiHeaders))
		return (this->makeErrorResponse("500 Internal Server Error (CGI output unreadable, body)"));
	if (statusCode.empty())
	{
		statusCode = "302";
		statusDesc = "Found";
	}
	else if (statusCode != "302")
		return (this->makeErrorResponse("500 Internal Server Error (CGI output credirdoc without code 302)"));
	return (this->generateCGIResponseWithDoc(cgiHeaders, statusCode, statusDesc));
}

// Generates the HTTP response, after CGI output is processed, for the most common case : no redirection
// Checks that headers are coherent and adds a status (with default value) if absent,
// 		then calls <generateCGIResponseWithDoc> to copy CGI output body inside response buffer
int		Response::generateCGIResponseNoredir(std::multimap<std::string, std::string>& cgiHeaders,
						std::string& statusCode, std::string& statusDesc)
{
	std::cout << "\t-> generating response without redirect, body of size " << this->_cgiOutputBody.size() << "\n";
	if (this->checkCGIOutputBody(cgiHeaders))
		return (this->makeErrorResponse("500 Internal Server Error (CGI output unreadable, body)"));
	if (statusCode.empty())
	{
		statusCode = "200";
		statusDesc = "OK";
	}
	return (this->generateCGIResponseWithDoc(cgiHeaders, statusCode, statusDesc));
}

///////////////////////////////////////
// HANDLING CGI - PROCESS CGI OUTPUT //
///////////////////////////////////////

// /!\ when parsing CGI output, there may be formatting errors in the output,
// 	   in such cases server should respond to client with "500 Internal Server Error"

// Goes through the <std::vector> of <char> storing the CGI output
// to find the double linebreak separating headers from body
// 		\ since CGI may use "\n" but also "\r\n" linebreaks,
// 		  this function searches for both and returns -1 if not found,
// 		  2 if found "\n" double linebreak, 4 if found "\r\n" double linebreak
// 		\ stores in <ind> the index of the first character of the double linebreak,
// 		  or std::string::npos if not found
int	Response::findCGIOutputHeader(size_t& ind)
{
	std::vector<char>&	output = this->_cgiOutput;
	size_t				siz = output.size();
	
	ind = 0;
	while (ind < siz)
	{
		if (ind + 1 < siz && output[ind] == '\n' && output[ind + 1] == '\n')
			return (2);
		if (ind + 3 < siz && output[ind] == '\r' && output[ind + 1] == '\n'
						  && output[ind + 2] == '\r' && output[ind + 3] == '\n')
			return (4);
		ind++;
	}
	ind = std::string::npos;
	return (-1);
}

// Parses the status line from the CGI output : searches for the separator,
// then the first element is the status code (must be valid HTTP return code)
// and the second element is the status description (which is only informational)
int	Response::parseCGIStatusLine(std::string& line, std::string& statusCode, std::string& statusDesc)
{
	unsigned long	indBeginCode;
	unsigned long	indEndCode;
	std::string		codeLit;

	indBeginCode = line.find(':', 0) + 1; // no need to check for npos, ':' must be present
	while (indBeginCode < line.size() && line[indBeginCode] == ' ')
		indBeginCode++;
	if (indBeginCode >= line.size())
		return (logError("\tstatus line has no values", 0));
	indEndCode = line.find(' ', indBeginCode);
	if (indEndCode == std::string::npos || indEndCode == indBeginCode)
		return (logError("\tstatus line has no description string", 0));
	codeLit = line.substr(indBeginCode, indEndCode - indBeginCode);
	if (invalidReturnCode(codeLit))
		return (logError("\tstatus line has invalid code " + itostr(codeLit), 0));
	statusCode = codeLit;
	statusDesc = line.substr(indEndCode + 1);
	return (0);
}

// Parses a header line from the CGI output :
// 		\ searches for the separator ':'
// 		\ extracts the key from left part and minimizes all characters
// 			(because headers are case-insensitive,
// 			 eg. PHP writes "Content-length" instead of "Content-Length")
// 		\ discards potential spaces and extracts value from left part
// 		\ inserts pair of key and value into the headers multimap
// There may be multiple "Set-Cookie" headers (hence multimap is used)
// 		but other header names must be unique
int	Response::parseCGIHeaderLine(std::string& line, std::multimap<std::string, std::string>& cgiHeaders)
{
	std::string		key;
	std::string		value;
	unsigned long	indSep;

	indSep = line.find(':');
	if (indSep == std::string::npos || indSep == 0)
		return (logError("\theader line has no separator", 0));
	key = line.substr(0, indSep);
	std::transform(key.begin(), key.end(), key.begin(), &tolower);
	if (cgiHeaders.count(key) > 0 && key != "set-cookie")
		return (logError("\tmultiple headers with same name and not 'Set-Cookie'", 0));
	indSep++;
	while (indSep < line.size() && line[indSep] == ' ')
		indSep++;
	if (indSep >= line.size())
		return (logError("\theader line has no value", 0));
	value = line.substr(indSep);
	cgiHeaders.insert(std::pair<std::string, std::string>(key, value));
	return (0);
}

// Parses the headers part of CGI output, formatted like an HTTP request, except
// 		\ line separators may be either '\n' or '\r\n', so <findCGIOutputHeader> searches for both
// 			and once headers string is extracted the combination <erase, remove> removes all '\r' from it
// 		\ there is no 'path' line but there may be a 'Status' header
// 			that is not necessarily the first line, and is not mandatory
// Searches for the double linebreak that separates the headers from the body,
// stores the body in a Response class member variable and puts the header in a string,
// 		then loops through the header lines and calls the appropriate functions to parse them
// 			(information in the 'Status' line goes into variables <statusCode> and <statusDesc>)
int	Response::parseCGIHeaders(std::multimap<std::string, std::string>& cgiHeaders,
	std::string& statusCode, std::string& statusDesc)
{
	size_t				posEndHeaders;
	int					linebreakType;
	std::string			headersString;
	std::stringstream	parserStream;
	std::string			line;

	// Locate separation between headers and (optional) body
	linebreakType = this->findCGIOutputHeader(posEndHeaders);
	std::cout << "\tposition of linebreak " << posEndHeaders << " / " << this->_cgiOutput.size()
		<< ", type of linebreak : " << linebreakType << "\n";
	if (linebreakType == -1)
		return (logError("[Res::parseCGIout] headers end not found", 0));
	headersString = std::string(this->_cgiOutput.begin(), this->_cgiOutput.begin() + posEndHeaders + linebreakType);
	headersString.erase(std::remove(headersString.begin(), headersString.end(), '\r'), headersString.end());
	this->_cgiOutputBody.clear();
	if (posEndHeaders + linebreakType == this->_cgiOutput.size())
		std::cout << "\tCGI output has no body\n";
	else
	{
		this->_cgiOutputBody = std::vector<char>(this->_cgiOutput.begin() + posEndHeaders + linebreakType,
			this->_cgiOutput.end());
		std::cout << "\tCGI output has body of size " << this->_cgiOutputBody.size() << "\n";
	}

	// Parse each header line
	parserStream << headersString;
	while (std::getline(parserStream, line))
	{
		if (line.find("Status:", 0) == 0)
		{
			if (!(statusCode.empty()) || this->parseCGIStatusLine(line, statusCode, statusDesc))
				return (1);
		}
		else if ((!line.empty()) && this->parseCGIHeaderLine(line, cgiHeaders))
			return (1);
	}
	return (0);
}

// Log function for conveniency
void	logCGIHeaders(std::multimap<std::string, std::string>& cgiHeaders,
	std::string& statusCode, std::string& statusDesc, std::vector<char>& cgiOutputBody)
{
	std::map<std::string, std::string>::iterator	it;
	unsigned int									ind;

	std::cout << "\033[33m< HEADERS\n";
	std::cout << "Status code = " << statusCode << ", desc = " << statusDesc << "\n";
	for(it = cgiHeaders.begin(); it != cgiHeaders.end(); it++)
		std::cout << (*it).first << " = " << (*it).second << "\n";
	std::cout << "\n>\n< BODY\n";
	for(ind = 0; ind < cgiOutputBody.size(); ind++)
		std::cout << cgiOutputBody[ind];
	std::cout << "\n>\n\033[0m";
}

// Log function for conveniency
void	logCGIOutput(std::vector<char> cgiOutput)
{
	unsigned int		ind;

	std::cout << "\033[32m<\n";
	for(ind = 0; ind < cgiOutput.size(); ind++)
		std::cout << cgiOutput[ind];
	std::cout << "\n>\n\033[0m";
}

// Parses the CGI output, then determines the response case and calls the appropriate function
// 		\ if header "Location" present, with a body : client redirection with document attached
// 		\ if header "Location" present, giving a local path (beginning with '/') : local redirection
// 		\ if header "Location" present, giving a new host+URL (beginning with 'http://') : client redirection
// 		\ if header "Location" absent, simple response with a document (most important case)
int	Response::processCGIOutput()
{
	std::multimap<std::string, std::string>	cgiHeaders;
	std::string								statusCode = "";
	std::string								statusDesc;
	std::string								locationValue;

	logCGIOutput(this->_cgiOutput);
	if (this->parseCGIHeaders(cgiHeaders, statusCode, statusDesc))
		return (this->makeErrorResponse("500 Internal Server Error (CGI output unreadable, headers)"));
	std::cout << "\textracted headers string from CGI output :\n";
	logCGIHeaders(cgiHeaders, statusCode, statusDesc, this->_cgiOutputBody);

	if (cgiHeaders.count("location") == 1)
	{
		locationValue = (*(cgiHeaders.find("location"))).second;
		if (locationValue.size() == 0)
			return (this->makeErrorResponse("500 Internal Server Error (CGI output unreadable, location)"));
		else if (this->_cgiOutputBody.size() > 0)
			return (this->generateCGIResponseClientRedirectDoc(cgiHeaders, statusCode, statusDesc));
		else if (locationValue[0] == '/')
			return (this->generateCGIResponseLocalRedirect(cgiHeaders, statusCode, statusDesc));
		else
			return (this->generateCGIResponseClientRedirect(cgiHeaders, statusCode, statusDesc));
	}
	else
		return (this->generateCGIResponseNoredir(cgiHeaders, statusCode, statusDesc));
}

//////////////////////////////////
// HANDLING CGI - CHILD PROCESS //
//////////////////////////////////

// Defines all the environment variable required by the CGI convention,
// they will be accessed by the CGI script to determine its output
// 		\ identification of parties : server name and port number, client IP address
// 		\ script information : path info, script name, request URI (all are simply <filePath> from request)
// 			/!\ PATH_INFO is the request's <path_info>, because subject requires to handle CGI requests
// 				as leading to a script to be interpreted rather than an executable to `execve`, cf 'main' comments
// 					(REDIRECT_STATUS set to 200 is used to indicate php-cgi that this is the chosen method)
// 		\ parameters given by client : query string, request method
// 									 + body type and length if there is a body
// 		\ programs information : CGI version, HTTP version, server software
// 		\ special cases in request headers to be transmitted to CGI with adapted env name and "HTTP_" prefix:
// 			~ "implementation defined" headers beginning with "X_"
// 			~ header transmitting cookie values "Cookie"
void	Response::setupCGIEnv(std::string& fullPath, std::map<std::string, std::string>& env)
{
	std::map<std::string, std::string>::iterator	it;
	std::map<std::string, std::string>&				headers = this->_request->_headers;
	in_addr											clientAddress;
	std::string										speHeader;

	(void)fullPath;
	clientAddress.s_addr = htonl(this->_clientPortaddr.first);
	env["PATH_INFO"] = this->_request->_filePath;
	env["PATH_TRANSLATED"] = fullPath;
	env["SCRIPT_NAME"] = this->_request->_filePath;
	env["REQUEST_URI"] = this->_request->_filePath;
	env["QUERY_STRING"] = this->_request->_queryString;
	env["REQUEST_METHOD"] = this->_request->_method;
	if (this->_request->_method == "POST")
	{
		env["CONTENT_LENGTH"] = itostr(this->_request->_bodySize);
		env["CONTENT_TYPE"] = headers["Content-Type"];
		// std::cout << "CGI CHILD env for POST request :\n"
		// 	<< "\tCONTENT_LENGTH to " << this->_request->_body.size() << "\n"
		// 	<< "\tCONTENT_TYPE to " << headers["Content-Type"] << "\n"
		// 	<< "\tREQUEST_METHOD to " << this->_request->_method << "\n";
	}
	env["GATEWAY_INTERFACE"] = "CGI/1.1";
	env["REMOTE_ADDR"] = std::string(inet_ntoa(clientAddress));
	env["SERVER_NAME"] = this->_request->_hostName; // /!\ empty if request used IP instead of domain name
	env["SERVER_PORT"] = itostr(this->_request->_mainSocket.portaddr.second);
	env["SERVER_PROTOCOL"] = "HTTP/1.1";
	env["SERVER_SOFTWARE"] = "Webserv";
	env["REDIRECT_STATUS"] = itostr(200);
	for (it = headers.begin(); it != headers.end(); it++)
	{
		speHeader = (*it).first;
		if ((speHeader.size() >= 2 && speHeader[0] == 'X' && speHeader[1] == '-')
			|| speHeader == "Cookie")
		{
			transformToEnvName(speHeader);
			speHeader = "HTTP_" + speHeader;
			env[speHeader] = (*it).second;
		}
	}
}

// Transforms the environment <std::map> prepared by <setupCGIEnv>
// into a char* array of elements of form "KEY=VALUE", and returns it
// 		(result of <new> not checked as usual)
char**	Response::createEnvArray(std::string& fullPath, int* envArraySize)
{
	std::map<std::string, std::string>				env;
	std::map<std::string, std::string>::iterator	envIt;
	char**											envArray;
	unsigned int									ind;
	char*											envVar;
	std::string										envVarStr;

	this->setupCGIEnv(fullPath, env);
	ind = 0;
	envArray = new char*[env.size() + 1];
	envArray[env.size()] = NULL;
	for (envIt = env.begin(); envIt != env.end(); envIt++)
	{
		envVarStr = (*envIt).first + std::string("=") + (*envIt).second;
		envVar = duplicateCstr(envVarStr.c_str());
		envArray[ind] = envVar;
		ind++;
	}
	*envArraySize = env.size();
	return (envArray);
}

// (Function called in the child process) Prepares and makes the call to <execve>,
// using <fullPath> as the path to the script and <interpreterPath> as the path to its interpreter
// 		\ change directory to directory where the script to execute is located
// 		\ redirect 'stdin' (request body received from <fdInput>, receiving end of pipe)
// 				   'stdout' (CGI output sent to <fdOutput>, open temporary file)
// 		\ close all other file descriptors (there will be many open socket file descriptors)
// 		\ setup arrays holding arguments to the main :
// 			~ <envArray> with env variables
// 			~ <argv> with path to interpreter as <argv[0]>, path to script at <argv[1]>
// /!\ Arrays allocation done in C++ style with <new>/<delete>, even if <execve> is a C function,
// 	   this does not create problems because <execve> does not care how memory was allocated
void	Response::childExecuteCGI(std::string& fullPath, std::string& interpreterPath,
	int fdInput, int fdOutput)
{
	char**			argv;
	char**			envArray;
	int				envArraySize; // does not include terminating NULL
	std::string		scriptName;
	std::string		scriptDir;

	std::cout << "CGI CHILD : executes with input from fd " << fdInput
		<< ", output to fd " << fdOutput << "\n";
	if (divideFilePath(fullPath, scriptDir, scriptName) || chdir(scriptDir.c_str()))
	{
		logError("CGI CHILD : Error: 'chdir' failure", 1);
		exit(127);
	}
	std::cout << "CGI CHILD : successfully changed directory to " << scriptDir << "\n";
	if (dup2(fdInput, 0) == -1 || dup2(fdOutput, 1) == -1)
	{
		logError("CGI CHILD : Error: 'dup2' failure", 1);
		exit(127);
	}
	closefrom(3);
	argv = new char*[3 * sizeof(char *)];
	argv[0] = duplicateCstr(interpreterPath.c_str());
	argv[1] = duplicateCstr(fullPath.c_str());
	argv[2] = NULL;
	envArray = this->createEnvArray(fullPath, &envArraySize);
	if (execve(argv[0], argv, envArray))
	{
		free2dimArray(argv, 1);
		free2dimArray(envArray, envArraySize);
		logError("CGI CHILD : Error: call to 'execve' failed", 1);
		exit(127);
	}
}

///////////////////////////////////
// HANDLING CGI - PARENT PROCESS //
///////////////////////////////////

// When there is a request body, sends it to the child on <fdOutput> (the sending end of <pipeRequest>)
// by parts of size <WRITEPIPE_BUFFER_SIZE> until the whole body has been sent
int		Response::feedBodyToChild(int fdOutput)
{
	long			nBytesWritten;
	unsigned long	nBytesToWrite;
	unsigned long	posInBody = 0;
	unsigned long	bodySize;

	bodySize = this->_request->_bodySize;
	std::cout << "\tfeeding POST request body of size " << bodySize << " to CGI child\n";
	while (posInBody < bodySize)
	{
		nBytesToWrite = std::min(WRITEPIPE_BUFFER_SIZE, bodySize - posInBody);
		nBytesWritten = write(fdOutput, this->_request->_body + posInBody, nBytesToWrite);
		if (nBytesWritten <= 0)
			return (close(fdOutput), 1);
		posInBody += nBytesWritten;
		std::cout << "\tfed : " << posInBody << " / " << bodySize << "\n";
	}
	std::cout << "\tall POST body fed\n";
	close(fdOutput);
	return (0);
}

// When the CGI child terminated (detected by reading EOF on CGI output pipe),
// uses <waitpid> to determine if it exited normally or encountered an error,
// 		then processes CGI output if all went well
// -> a lot of error handling, but (as usual) errors on in CGI child ARE NOT FATAL to Webserv :
// 		if there was an error/interruption in child, simply generate an error response
// (<waitRet> return value 0 means CGI child did not actually terminate, this should never happen)
int		Response::handleCGIFinished()
{
	int		exitStatus;
	pid_t	waitRet;

	std::cout << "[Res::CGI] EOF received on CGI output file, checking child exit status\n";
	close(this->_cgiReadFd);
	waitRet = waitpid(this->_cgiPid, &exitStatus, 0);
	if (waitRet == -1 || waitRet == 0)
	{
		logError("CGI wait error : " + itostr(waitRet), 1);
		return (this->makeErrorResponse("500 Internal Server Error (system call)"));
	}
	if (WIFEXITED(exitStatus))
		exitStatus = WEXITSTATUS(exitStatus);
	else
	{
		std::cout << "\tError: CGI program did not exit normally\n";
		return (this->makeErrorResponse("500 Internal Server Error (system call)"));
	}
	if (exitStatus == 127)
	{
		std::cout << "\tError: CGI program could not be executed\n";
		return (this->makeErrorResponse("500 Internal Server Error (system call)"));
	}
	std::cout << "\t[Res::CGI] CGI program terminated with exit status " << exitStatus << "\n";
	return (this->processCGIOutput());
}

// Called by <Client> instance when POLLIN occurred on <_cgiReadFd> where CGI output is expected
// 2 options :
// 		\ read on fd returns positive number -> CGI child still alive,
// 			so store the read bytes and continue to wait for output (return code 3)
// 		\ read on fd returns 0 (EOF) -> CGI child closed its end of the output pipe <=> it terminated,
// 			so check if it terminated successfully, in that case process stored output
// Unlike when receiving an HTTP request, CGI output has not header "Content-Length",
// so it must be read without knowing its content length AND without using <std::string>
// since the body may be binary data (eg. image)
// 		-> CGI output stored in a <std::vector> of char, that will manage the allocation
int		Response::checkCGIFinished()
{
	long	nBytesRead = 1;
	char	buffer[READPIPE_BUFFER_SIZE + 1];

	bzero(buffer, READPIPE_BUFFER_SIZE + 1);
	nBytesRead = read(this->_cgiReadFd, buffer, READPIPE_BUFFER_SIZE);
	if (nBytesRead == -1)
	{
		logError("error of 'read' on CGI output", 1);
		kill(this->_cgiPid, SIGINT);
		return (this->makeErrorResponse("500 Internal server error (system call)"), 1);
	}
	else if (nBytesRead == 0)
		return (this->handleCGIFinished());
	else
	{
		this->_cgiOutput.insert(this->_cgiOutput.end(), buffer, buffer + nBytesRead);
		return (3);
	}
}

// Sends the request body (if there is one) to the child,
// then stores the child process' PID and fd of receiving end of CGI output pipe in member variables
// Returns 3 to indicate client that response will be ready after CGI script termination
// 		-> <Client> instance will now include the pipe fd in the fds surveiled by poll
// 		   and call <Response.checkCGIfinished> each time a POLLIN occurs on that fd :
// 			this way the CGI output will be read preogressively until it is done	
int		Response::startWaitCGI(pid_t pid, int bodyWriteFd, int cgiReadFd)
{
	// Transmit body
	std::cout << "\tCGI child successfully forked\n";
	if (this->_request->_method == "POST" && this->_request->_bodySize > 0)
	{
		if (this->feedBodyToChild(bodyWriteFd))
		{
			kill(pid, SIGINT);
			std::cout << "\tError: unable to feed body to CGI program\n";
			return (this->makeErrorResponse("500 Internal Server Error (system call)"));
		}
	}
	else
		close(bodyWriteFd);

	// Store PID and fd of receiving end of CGI output pipe
	this->_cgiPid = pid;
	this->_cgiReadFd = cgiReadFd;
	std::cout << "\tPID " << this->_cgiPid << " and pipe fd " << this->_cgiReadFd << " stored, starting wait\n";
	return (3);
}

// Creates the interfaces for communicating with CGI script
// 		\ a pipe to send the request body (stdin of CGI child)
// 		\ a pipe ro receive the CGI output (stdout of CGI child)
// then calls <fork> to create the child that will execute the CGI script
int		Response::forkCGI(std::string& fullPath, std::string& interpreterPath)
{
	int				pipeReqbody[2];
	int				pipeCGIout[2];
	pid_t			pid;

	if (pipe(pipeReqbody) == -1)
		return (logError("Error: unable to create pipe for CGI\n", 1),
			this->makeErrorResponse("500 Internal Server Error (system call)"));
	if (pipe(pipeCGIout) == -1)
		return (logError("Error: unable to create pipe for CGI\n", 1),
			this->makeErrorResponse("500 Internal Server Error (system call)"));
	pid = fork();
	if (pid == -1)
		return (logError("Error: unable to fork child for CGI\n", 1),
			this->makeErrorResponse("500 Internal Server Error (system call)"));
	else if (pid == 0)
	{
		close(pipeReqbody[1]);
		close(pipeCGIout[0]);
		this->childExecuteCGI(fullPath, interpreterPath, pipeReqbody[0], pipeCGIout[1]);
		return (0);
	}
	else
	{
		close(pipeReqbody[0]);
		close(pipeCGIout[1]);
		return (this->startWaitCGI(pid, pipeReqbody[1], pipeCGIout[0]));
	}
}

// CGI (Common Gateway Interface) is the convention by which a client can ask
// for the execution of a script/program on the server side
// This function is called only if
// 		(a) the config file allowed Webserv to perform CGI (ie. execute scripts)
// 			at the location targeted by the request
// 		(b) the request path to the script ends with an extension allowed for CGI
// 			<=> for which the config file defined an intepreter
// 				(in practice, '.php' or '.py')
// This function then checks that the CGI script given by the request path exists,
// that the interpreter from config file exists and is executable, then starts the CGI process :
// 		\ fork a child process that that will use <execve> to launch the script
// 			with its 'stdout' redirected towards a temp file + its env variables set as parameters
// 		\ read the CGI output in the temp file (then delete temp file), and store it
// 		\ parse the CGI output (containing headers and a body, like a HTTP request)
// 		\ produce the HTTP response that will be sent to the client
// 			(4 potential response cases according to : local/client/no redirection, with/without body)
int	Response::handleCGI()
{
	std::string	fullPath;
	std::string	interpreterPath;

	if (this->_request->_toDir)
		return (this->makeErrorResponse("404 Not Found (CGI to a directory)"));
	interpreterPath = this->_location->cgiInterpreted[this->_request->_extension];
	std::cout << "\tPath to interpreter : " << interpreterPath << "\n";
	if (access(interpreterPath.c_str(), X_OK) != 0)
		return (logError("CGI interpreter", 1), this->makeErrorResponse("500 Internal Server Error (invalid CGI intepreter)"));
	if (this->pathToFile(fullPath, 1, 0, NULL))
		return (this->makeErrorResponse("404 Not Found (CGI file not found)"));
	return (this->forkCGI(fullPath, interpreterPath));
}
