#include "../hpp_files/webserv.hpp"

///////////////////////////////////////////
// HANDLING GET REQUESTS - FILE RESPONSE //
///////////////////////////////////////////

// Opens the requested resource, then reads all of it and copies its content in <buffer>
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

// Generates a response containing a file in its body,
// 		\ generates headers ("Content-Type" header determined by extension)
// 		\ allocates a sufficient buffer (headers size + <fileSize>)
// 		\ concatenates headers and resource content in <_responseBuffer>
int	Response::makeFileResponse(std::string& fullPath, size_t fileSize, std::string status)
{
	std::stringstream	headersStream;
	std::string			headersString;
	size_t				totalSize;

	std::cout << "\tsending regular file at : " << fullPath << "\n"
		<< "\t\twith Content-Type : " << this->getResponseBodyType() << "\n";
	// Create HTTP format headers for response
	headersStream << "HTTP/1.1 " << status << "\r\n"
			 << "Content-Type: " << this->getResponseBodyType() << "\r\n"
			 << "Content-Length: " << fileSize << "\r\n"
			 << "\r\n";
	headersString = headersStream.str();
	totalSize = headersString.size() + fileSize;
	std::cout << "[Res::fileResponse] headers of size " << headersString.size() << " + file of size "
		<< fileSize << " = total size " << totalSize << " to be allocated\n";
	// Allocate response buffer with enough space for whole file content
	this->_responseBuffer = new char[totalSize + 1];
	bzero(this->_responseBuffer, totalSize);
	strcpy(this->_responseBuffer, headersString.c_str());
	if (this->readFullFileInBuffer(fullPath, fileSize, this->_responseBuffer + headersString.size()))
	{
		delete[] this->_responseBuffer; this->_responseBuffer = NULL;
		return (makeErrorResponse("500 Internal Server Error (system call)"));
	}
	this->_responseSize = totalSize;
	return (0);
}

///////////////////////////////////////
// HANDLING GET REQUESTS - DIRECTORY //
///////////////////////////////////////

// Generates in <body> the HTML page giving the content of directory at <fullPath>
// using function <read/close/opendir> to explore the directory
int	Response::generateDefaultIndexHtml(std::string& fullPath, std::string& body)
{
	std::stringstream	ss;
	struct __dirstream	*dirPointer;
	struct dirent		*dirContent;

	dirPointer = opendir(fullPath.c_str());
	if (!dirPointer)
		return (1);
	dirContent = readdir(dirPointer);

	ss << "<!DOCTYPE html>\n<html>\n"
		<< "<head>\n<meta charset=\"UTF-8\">\n<title>Default index</title>\n</head>\n"
		<< "<body>\n<h1>Default index of directory : " << this->_request->_filePath << "</h1>\n"
		<< "<ul>\n";
	while (dirContent)
	{
		if (dirContent->d_type == DT_REG)
		{
			ss << "<li><a href=\"" << dirContent->d_name << "\">"
				<< dirContent->d_name << "</a></li>\n";
		}
		else if (dirContent->d_type == DT_DIR
			&& strcmp(dirContent->d_name, ".") != 0 && strcmp(dirContent->d_name, "..") != 0)
		{
			ss << "<li><a href=\"" << dirContent->d_name << "/\">"
				<< dirContent->d_name << "/</a></li>\n";
		}
		dirContent = readdir(dirPointer);
	}
	ss << "</ul>\n</body>\n</html>\n";
	closedir(dirPointer);
	body = ss.str();
	return (0);
}

// Assembles the headers and body for a response containing
// a simple HTML page giving the content of a directory
int	Response::makeAutoindexResponse(std::string& fullPath)
{
	std::stringstream	ss;
	std::string			responseStr;
	std::string			body;

	if (this->generateDefaultIndexHtml(fullPath, body))
		return (this->makeErrorResponse("500 Internal Server Error"));
	ss << "HTTP/1.1 200 OK\r\n"
		<< "Content-Length: " << body.size() << "\r\n"
		<< "Content-Type: text/html" << "\r\n\r\n"
		<< body;
	responseStr = ss.str();
	this->makeResponseFromString(responseStr);
	return (0);
}

// Handles GET requests with no CGI on a path ending with '/' <=> a path to a directory
// A lot of conditions in the function to handle directives 'index' and 'autoindex' from cfg file
// 		\ compute <fullPath> and check that it really leads to a directory with <pathToFile>
// 			(should return 2 ; return value of 0 means that path leads to normal file)
// 		\ check that path targets a location (index responses allowed only inside a location)
// 		\ define the path <index> to default index file "index.html", or index file defined by cfg file
// 			then if that file exists, perform local redirect to it (return value 2 for <Client> instance)
// 		\ finally when no index file present and cfg file allowed auto indexing,
// 			respond with index file generated by <makeAutoindexResponse>
// [cfg feature] define file to serve when request targets directory ; autoindex
int	Response::handleGetOnDir()
{
	std::string			indexFile;
	std::string			indexPath;
	std::string			fullPath;
	int					ret;
	//struct stat			fileStat;
	//size_t				fileSize;

	eraseLastChar(this->_request->_filePath);
	std::cout << "\tGET request on directory : " << this->_request->_filePath << "\n";
	ret = this->pathToFile(fullPath, 1, 0, NULL);
	if (ret == 0)
		return (this->makeRedirResponse("303 See Other", this->_request->_filePath), 0);
	if (ret == 1)
		return (this->makeErrorResponse("404 Not Found"));
	if (!(this->_location))
		return (this->makeErrorResponse("404 Not Found (requested index file outside location)"));

	if (!(this->_location->fileWhenDir.empty()))
		indexFile = this->_location->fileWhenDir;
	else
		indexFile = std::string("index.html");
	indexPath = fullPath + "/" + indexFile;
	std::cout << "\tpath to potential index file : " << indexPath << "\n";
	if (access(indexPath.c_str(), F_OK) == 0)
	{
		this->_request->redirectPath(this->_request->_filePath + "/" + indexFile, "GET");
		return (2);
	}
	else if (this->_location->autoIndex)
	{
		std::cout << "\tpotential index file does not exist, auto index applied\n";
		return (this->makeAutoindexResponse(fullPath));
	}
	else
		return (this->makeErrorResponse("404 Not Found (no index file available)"));
	return (0);
}

///////////////////////////
// HANDLING GET REQUESTS //
///////////////////////////

// Handles GET requests with no CGI -> the request asks for the content of a specific file 
// Checks that the requested resource is not a directory and that it exists,
// before generating the response containing its content
// (if the requested resource is a directory
// 		\ with the path correctly ending with '/' (<_toDir> set) : passes <handleGetOnDir>
// 		\ with the path not ending with '/' (<pathToFile> returns 2) : responds with a redirection
// 			showing the correct path to the directory, ending with a '/'
int	Response::handleGet()
{
	std::string	fullPath;
	size_t		fileSize;
	int			ret;

	if (this->_request->_toDir)
		return (this->handleGetOnDir());
	ret = this->pathToFile(fullPath, 1, 0, &fileSize);
	if (ret == 2)
		return (this->makeRedirResponse("303 See Other", ""));
	else if (ret)
		return (this->makeErrorResponse("404 Not Found"));
	if (fileSize > FILEDOWNLOAD_MAXSIZE)
		return (this->makeErrorResponse("413 Content Too Large"));
	return (this->makeFileResponse(fullPath, fileSize, "200 OK"));
}
