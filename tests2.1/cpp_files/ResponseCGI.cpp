#include "../hpp_files/webserv.hpp"

//////////////////////////////////////
// HANDLING CGI - GENERATE RESPONSE //
//////////////////////////////////////

// Generates the HTTP response, after CGI output is processed, for case : client redirection
// 		(<=> indicate the client to send a new request, with the path given in "Location" instead)
// Checks that CGI output contains only the "Location" header and that status code is 302, as required by RFC
int		Response::generateCGIResponseClientRedirect(std::map<std::string, std::string>& cgiHeaders,
						std::string& statusCode, std::string& statusDesc)
{
	std::stringstream	ss;
	std::string			responseStr;

	std::cout << "\t-> generating response for client redirect, without document\n";
	if (cgiHeaders.size() > 1 || this->_cgiOutputBody.size() > 0)
		return (this->makeErrorResponse("500 Internal Server Error (CGI output credir with non-Location headers)"), 0);
	if (statusCode.empty())
	{
		statusCode = "302";
		statusDesc = "Found";
	}
	else if (statusCode != "302")
		return (this->makeErrorResponse("500 Internal Server Error (CGI output credir without code 302)"), 0);

	ss << "HTTP/1.1 " << statusCode << " " << statusDesc << "\r\n"
		<< "Content-Length: 0" << "\r\n"
		<< "Location: " << cgiHeaders["Location"] << "\r\n\r\n";
	responseStr = ss.str();
	this->makeResponseFromString(responseStr);
	return (0);
}

// Handles, after CGI output is processed, the case : local redirection
// Although the function is named <generateCGIResponse...> for coherence,
// it does not create an HTTP response in <_responseBuffer>, instead
// 		\ it checks that CGI output contains only the "Location" header as required by RFC
// 		\ updates the path in the instance of <Request> to be the path delivered by the CGI
// 		\ returns special return value 2 to indicate the calling instance of <Client>
// 			that the request must be reprocessed (with a new <Response> instance) because of redirection
int		Response::generateCGIResponseLocalRedirect(std::map<std::string, std::string>& cgiHeaders,
						std::string& statusCode, std::string& statusDesc)
{
	std::string		responseStr;
	std::string&	newPath = cgiHeaders["Location"];

	(void)statusCode; (void)statusDesc;
	std::cout << "\t-> generating response for local redirect to " << newPath << "\n";
	if (cgiHeaders.size() > 1 || this->_cgiOutputBody.size() > 0)
		return (this->makeErrorResponse("500 Internal Server Error (CGI output lredir with non-Location headers)"), 0);
	if (this->_request->redirectPath(newPath))
		return (this->makeErrorResponse("500 Internal Server Error (CGI output lredir with invalid new path)"), 0);
	return (2);
}

// Checks the coherence of CGI output headers
// ("Content-Length" must be absent, "Content-Type" present when there is a body)
int Response::checkCGIOutputBody(std::map<std::string, std::string>& cgiHeaders)
{
	if (cgiHeaders.count("Content-Length") > 0)
		return (logError("\tError: CGI defined header Content-Length", 0));
	if (this->_cgiOutputBody.size() > 0 && cgiHeaders.count("Content-Type") == 0)
		return (logError("\tError: CGI did not define header Content-Type", 0));
	return (0);
}

// For CGI requests resulting in a document response with/without redirect,
// allocates the response buffer, then concatenates in it :
// 	  the status line + all header lines + the CGI output body (if there is one)
// (the CGI output body content was stored as <std::vector> of characters,
// 		so its bytes are added one by one to the response buffer)
int	Response::generateCGIResponseWithDoc(std::map<std::string, std::string>& cgiHeaders,
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
	headersStream << "\n";
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
int		Response::generateCGIResponseClientRedirectDoc(std::map<std::string, std::string>& cgiHeaders,
						std::string& statusCode, std::string& statusDesc)
{
	std::cout << "\t-> generating response for client redirect, with document\n";
	if (this->checkCGIOutputBody(cgiHeaders))
		return (this->makeErrorResponse("500 Internal Server Error (CGI output unreadable)"), 0);
	if (statusCode.empty())
	{
		statusCode = "302";
		statusDesc = "Found";
	}
	else if (statusCode != "302")
		return (this->makeErrorResponse("500 Internal Server Error (CGI output credirdoc without code 302)"), 0);
	return (this->generateCGIResponseWithDoc(cgiHeaders, statusCode, statusDesc));
}

// Generates the HTTP response, after CGI output is processed, for the most common case : no redirection
// Checks that headers are coherent and adds a status (with default value) if absent,
// 		then calls <generateCGIResponseWithDoc> to copy CGI output body inside response buffer
int		Response::generateCGIResponseNoredir(std::map<std::string, std::string>& cgiHeaders,
						std::string& statusCode, std::string& statusDesc)
{
	std::cout << "\t-> generating response without redirect, body of size " << this->_cgiOutputBody.size() << "\n";
	if (this->checkCGIOutputBody(cgiHeaders))
		return (this->makeErrorResponse("500 Internal Server Error (CGI output unreadable)"), 0);
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
// Returns the index of the first '\n' in the double linebreak
// (return value 0 also serves as error indicator,
//  since there is an error if double linebreak absent, and also if present at position 0)
unsigned long	Response::findCGIOutputHeader()
{
	unsigned long	ind = 0;

	while (ind + 1 < this->_cgiOutput.size())
	{
		if (this->_cgiOutput[ind] == '\n' && this->_cgiOutput[ind + 1] == '\n')
			return (ind);
		ind++;
	}
	return (0);
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

// Parses a header line from the CGI output : searches for the separator ':',
// discards potential spaces, then puts left part as KEY and right part as value
int	Response::parseCGIHeaderLine(std::string& line, std::map<std::string, std::string>& cgiHeaders)
{
	std::string		key;
	std::string		value;
	unsigned long	indSep;

	indSep = line.find(':');
	if (indSep == std::string::npos || indSep == 0)
		return (logError("\theader line has no separator", 0));
	key = line.substr(0, indSep);
	indSep++;
	while (indSep < line.size() && line[indSep] == ' ')
		indSep++;
	if (indSep >= line.size())
		return (logError("\theader line has no value", 0));
	value = line.substr(indSep);
	cgiHeaders[key] = value;
	return (0);
}

// Parses the headers part of CGI output, formatted like an HTTP request, except
// 		\ line separators is usual '\n' instead of '\r\n'
// 		\ there is no 'path' line but a 'Status' header
// 			that is not necessarily the first line, and is not mandatory
// Searches for the double linebreak that separates the headers from the body,
// stores the body in a Response class member variable and puts the header in a string,
// 		then loops through the header lines and calls the appropriate functions to parse them
// 			(information in the 'Status' line goes into variables <statusCode> and <statusDesc>)
int	Response::parseCGIHeaders(std::map<std::string, std::string>& cgiHeaders,
	std::string& statusCode, std::string& statusDesc)
{
	unsigned long		posEndHeaders;
	std::string			headersString;
	std::stringstream	parserStream;
	std::string			line;

	// Locate separation between headers and (optional) body
	posEndHeaders = this->findCGIOutputHeader();
	if (posEndHeaders == 0)
		return (1);
	headersString = std::string(this->_cgiOutput.begin(), this->_cgiOutput.begin() + posEndHeaders);
	this->_cgiOutputBody.clear();
	std::cout << "\tposition of linebreak " << posEndHeaders << " / " << this->_cgiOutput.size() << "\n";
	if (posEndHeaders == this->_cgiOutput.size() - 1)
		std::cout << "\tCGI output has no body\n";
	else
	{
		this->_cgiOutputBody = std::vector<char>(this->_cgiOutput.begin() + posEndHeaders + 2, this->_cgiOutput.end());
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
		else if (this->parseCGIHeaderLine(line, cgiHeaders))
			return (1);
	}
	return (0);
}

// Log function for conveniency
void	logCGIHeaders(std::map<std::string, std::string>& cgiHeaders,
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
	std::map<std::string, std::string>	cgiHeaders;
	std::string							statusCode = "";
	std::string							statusDesc;

	logCGIOutput(this->_cgiOutput);
	if (this->parseCGIHeaders(cgiHeaders, statusCode, statusDesc))
		return (this->makeErrorResponse("500 Internal Server Error (CGI output unreadable)"), 0);
	std::cout << "\textracted headers string from CGI output :\n";
	logCGIHeaders(cgiHeaders, statusCode, statusDesc, this->_cgiOutputBody);

	if (cgiHeaders.count("Location") == 1)
	{
		if (cgiHeaders["Location"].size() == 0)
			return (this->makeErrorResponse("500 Internal Server Error (CGI output unreadable)"), 0);
		else if (this->_cgiOutputBody.size() > 0)
			return (this->generateCGIResponseClientRedirectDoc(cgiHeaders, statusCode, statusDesc));
		else if (cgiHeaders["Location"][0] == '/')
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
// 		\ parameters given by client : path_info (a path given in request URL after path to script),
// 		  query string, request method, (if there is a body) body type and length
// 		\ programs information : CGI version, HTTP version, server software, script name
void	Response::setupCGIEnv(std::string& fullPath, std::map<std::string, std::string>& env)
{
	std::map<std::string, std::string>&				headers = this->_request->_headers;
	in_addr											clientAddress;

	clientAddress.s_addr = htonl(this->_clientPortaddr.first);
	env["PATH_INFO"] = this->_request->_pathInfo.c_str();
	env["QUERY_STRING"] = this->_request->_queryString.c_str();
	env["REQUEST_METHOD"] = this->_request->_method.c_str();
	if (this->_request->_method == "POST")
	{
		env["CONTENT_LENGTH"] = itostr(this->_request->_bodySize);
		env["CONTENT_TYPE"] = headers["Content-Type"].c_str();
		// std::cout << "CGI CHILD env for POST request :\n"
		// 	<< "\tCONTENT_LENGTH to " << this->_request->_body.size() << "\n"
		// 	<< "\tCONTENT_TYPE to " << headers["Content-Type"] << "\n"
		// 	<< "\tREQUEST_METHOD to " << this->_request->_method << "\n";
	}
	env["GATEWAY_INTERFACE"] = "CGI/1.1";
	env["REMOTE_ADDR"] = std::string(inet_ntoa(clientAddress));
	env["SCRIPT_NAME"] = fullPath.c_str();
	env["SERVER_NAME"] = this->_request->_hostName.c_str(); // /!\ empty if request used IP instead of domain name
	env["SERVER_PORT"] = itostr(this->_request->_mainSocket.portaddr.second).c_str();
	env["SERVER_PROTOCOL"] = "HTTP/1.1";
	env["SERVER_SOFTWARE"] = "Webserv";
}

// Transforms the environment <std::map> prepared by <setupCGIEnv>
// into a char* array of elements of form "KEY=VALUE", and returns it
// 		(result of <new> not checked as usual,
// 		 but <strdup> uses <malloc> so its result is checked)
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
		envVar = strdup(envVarStr.c_str());
		if (envVar == NULL)
			return (free2dimArray(envArray, ind), (char **)NULL);
		envArray[ind] = envVar;
		ind++;
	}
	*envArraySize = env.size();
	return (envArray);
}


// #f : chdir to the dir where CGI should be executed
// (Function called in the child process) Prepares and makes the call to <execve>
// 		\ redirect 'stdin' (request body received from <fdInput>)
// 		  and 'stdout' (CGI output sent to <fdOutput>)
// 		\ setup arrays holding arguments to the main :
// 		  <argv> with path to script as <argv[0]>, <envArray> with env variables
// /!\ Arrays allocation done in C++ style with <new>/<delete>, even if <execve> is a C function,
// 	   this does not create problems because <execve> does not care how memory was allocated
void	Response::childExecuteCGI(std::string& fullPath, int fdInput, int fdOutput)
{
	char**			argv;
	char**			envArray;
	int				envArraySize; // does not include terminating NULL

	std::cout << "CGI CHILD : executes with input from fd " << fdInput
		<< ", output to fd " << fdOutput << "\n";
	if (dup2(fdInput, 0) == -1 || dup2(fdOutput, 1) == -1)
	{
		logError("CGI CHILD : Error: 'dup2' failure", 1);
		exit(127);
	}
	argv = new char*[2 * sizeof(char *)];
	argv[0] = strdup(fullPath.c_str()); // #f : protection
	argv[1] = NULL;
	envArray = this->createEnvArray(fullPath, &envArraySize);
	if (!envArray)
	{
		free2dimArray(argv, 1);
		logError("CGI CHILD : Error: 'malloc' failure", 1);
		exit(127);
	}
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
// by parts of size <WRITEPIPE_BUFFER_SIZE> until the whole body has beed sent
int		Response::feedBodyToChild(int fdOutput)
{
	long			nBytesWritten;
	unsigned long	nBytesToWrite;
	unsigned long	posInBody = 0;
	unsigned long	bodySize;

	std::cout << "\tfeeding POST request body to CGI child\n";
	bodySize = this->_request->_bodySize;
	while (posInBody < bodySize)
	{
		nBytesToWrite = std::min(WRITEPIPE_BUFFER_SIZE, bodySize - posInBody);
		nBytesWritten = write(fdOutput, this->_request->_body + posInBody, nBytesToWrite);
		if (nBytesWritten <= 0)
			return (close(fdOutput), 1);
		posInBody += nBytesWritten;
	}
	close(fdOutput);
	return (0);
}

// Reads the CGI output on <fdInput> (the receiving end of <pipeResponse>)
// Unlike when receiving an HTTP request, CGI output has not header "Content-Length",
// so it must be read without knowing its content length AND without using <std::string>
// since the body may be binary data (eg. image)
// 		-> CGI output stored in a <std::vector> of char, that will manage the allocation
int		Response::readResponseFromChild(int fdInput)
{
	long	nBytesRead = 1;
	char	buffer[READPIPE_BUFFER_SIZE + 1];

	this->_cgiOutput.clear();
	while (nBytesRead > 0)
	{
		bzero(buffer, READPIPE_BUFFER_SIZE + 1);
		nBytesRead = read(fdInput, buffer, READPIPE_BUFFER_SIZE);
		if (nBytesRead == -1)
			return (close(fdInput), 1);
		else if (nBytesRead > 0)
			this->_cgiOutput.insert(this->_cgiOutput.end(), buffer, buffer + nBytesRead);
	}
	close(fdInput);
	return (0);
}

// Sends the request body (if there is one) to the child, reads and stores its output,
// then calls <waitpid> to check if child terminated without errors
// -> a lot of error handling, but errors on in CGI child ARE NOT FATAL to Webserv
// 		\ if there was an error/interruption in child, simply generate an error response
// 		\ else, process the output to create the HTTP response
int		Response::waitForChildCGI(pid_t pid, int *pipeRequest, int *pipeResponse)
{
	int		exitStatus;

	std::cout << "\tCGI child successfully forked\n";
	close(pipeRequest[0]);
	close(pipeResponse[1]);
	if (this->_request->_method == "POST" && this->_request->_bodySize > 0)
	{
		if (this->feedBodyToChild(pipeRequest[1]))
		{
			waitpid(pid, &exitStatus, 0);
			std::cout << "\tError: unable to feed body to CGI program\n";
			return (this->makeErrorResponse("500 Internal Server Error (system call)"), 0);
		}
	}
	else
		close(pipeRequest[1]);
	if (this->readResponseFromChild(pipeResponse[0]))
	{
		waitpid(pid, &exitStatus, 0);
		std::cout << "\tError: unable to read response from CGI program\n";
		return (this->makeErrorResponse("500 Internal Server Error (system call)"), 0);
	}
	waitpid(pid, &exitStatus, 0);
	if (WIFEXITED(exitStatus))
		exitStatus = WEXITSTATUS(exitStatus);
	else
	{
		std::cout << "\tError: CGI program did not exit\n";
		return (this->makeErrorResponse("500 Internal Server Error (system call)"), 0);
	}
	if (exitStatus == 127)
	{
		std::cout << "\tError: CGI program could not be executed\n";
		return (this->makeErrorResponse("500 Internal Server Error (system call)"), 0);
	}
	std::cout << "\t[Res::CGI] CGI program terminated with exit status " << exitStatus << "\n";
	return (this->processCGIOutput());
}

// Creates 2 pipes
// 		\ first to send the request body to the CGI script
// 		\ second to receive the output from the CGI script
// then calls <fork> to create an executor child
int		Response::forkCGI(std::string& fullPath)
{
	int		pipeRequest[2];
	int		pipeResponse[2];
	pid_t	pid;

	if (pipe(pipeRequest) == -1)
		return (logError("Error: unable to create pipe for CGI\n", 1),
			this->makeErrorResponse("500 Internal Server Error (system call)"), 0);
	if (pipe(pipeResponse) == -1)
		return (logError("Error: unable to create pipe for CGI\n", 1),
			this->makeErrorResponse("500 Internal Server Error (system call)"), 0);
	pid = fork();
	if (pid == -1)
		return (logError("Error: unable to fork child for CGI\n", 1),
			this->makeErrorResponse("500 Internal Server Error (system call)"), 0);
	else if (pid == 0)
	{
		close(pipeRequest[1]);
		close(pipeResponse[0]);
		this->childExecuteCGI(fullPath, pipeRequest[0], pipeResponse[1]);
		return (0);
	}
	else
		return (this->waitForChildCGI(pid, pipeRequest, pipeResponse));
}

// CGI (Common Gateway Interface) is the convention by which a client can ask
// for the execution of a script/program on the server side
// This function is called only if
// 		a) the config file allowed Webserv to perform CGI (ie. execute scripts)
// 		   at the location targeted by the request
// 		b) the request path to the script ends with an extension allowed for CGI
// 		   (in practice, '.php' or '.py')
// This function then checks that the CGI script given by the request path
// exists and is executable, then starts the CGI process :
// 		\ fork a child process that that will use <execve> to launch the script
// 			with its 'stdout' redirected towards a pipe + its env variables set as parameters
// 		\ read the CGI output at the receiving end of the pipe, and store it
// 		\ parse the CGI output (containing headers and a body, like a HTTP request)
// 		\ produce the HTTP response that will be sent to the client
// 			(4 potential response cases according to : local/client/no redirection, with/without body)
int	Response::handleCGI()
{
	std::string	fullPath;

	if (this->_request->_toDir)
		return (this->makeErrorResponse("404 Not Found (CGI to a directory)"), 0);
	if (this->pathToFile(fullPath, 1, 1, NULL))
		return (this->makeErrorResponse("404 Not Found (CGI executable not found)"), 0);
	return (this->forkCGI(fullPath));
}
