#include "../hpp_files/webserv.hpp"

///////////////////////////
// HANDLING GET REQUESTS //
///////////////////////////

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

// Measures size of requested resource in <fileSize>, generates the response headers,
// allocates <_responseBuffer> to hold all headers plus the resource content,
// 		then concatenates headers and resource content in <_responseBuffer>
int	Response::makeFileResponse(std::string& fullPath, size_t fileSize)
{
	std::stringstream	headersStream;
	std::string			headersString;
	size_t				totalSize;

	// Create HTTP format headers for response
	headersStream << "HTTP/1.1 200 OK\r\n"
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
		return (makeErrorResponse("500 Internal Server Error (system call)"), 0);
	}
	this->_responseSize = totalSize;
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

int	Response::handleGetOnDir()
{
	this->makeErrorResponse("404 Not Found (GET on directory)");
	return (0);
}

// Handles GET requests with no CGI -> the request asks for the content of a specific file 
// Checks that the requested resource exists before generating the response containing its content
// (if the requested resource is a directory, responds with a redirection
//  showing the correct path to the directory, ending with a '/')
int	Response::handleGet()
{
	std::string	fullPath;
	size_t		fileSize;
	int			ret;

	if (this->_request->_toDir)
		return (this->handleGetOnDir());
	ret = this->pathToFile(fullPath, 1, 0, &fileSize);
	if (ret == 2)
		return (this->makeRedirResponse("303 See Other", ""), 0);
	else if (ret)
		return (this->makeErrorResponse("404 Not Found"), 0);
	if (fileSize > FILEDOWNLOAD_MAXSIZE)
		return (this->makeErrorResponse("413 Content Too Large"), 0);
	return (this->makeFileResponse(fullPath, fileSize));
}
