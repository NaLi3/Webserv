#include "../hpp_files/webserv.hpp"

//////////////////////////////////////////////
// HANDLING POST REQUESTS - UPLOAD ONE FILE //
//////////////////////////////////////////////

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

int	Response::uploadFile(std::string& fullPath, unsigned long fileSize,
	char* fileContent, bool makeResponse)
{
	bool		fileExists;

	if (fileSize > FILEUPLOAD_MAXSIZE)
		return (this->makeErrorResponse("413 Content Too Large"), 0);
	fileExists = (access(fullPath.c_str(), F_OK) == 0);
	std::cout << "\tuploading file to " << fullPath
		<< "\n\tfile already existed : " << fileExists << "\n";
	if (this->writeFullBufferInFile(fullPath, fileSize, fileContent))
		return (this->makeErrorResponse("500 Internal Server Error (system call)"), 0);
	std::cout << "\tfile upload done\n";
	if (makeResponse)
	{
		if (fileExists)
			this->makeSuccessResponse("200 OK");
		else
			this->makeSuccessResponse("201 Created");
	}
	return (0);
}

////////////////////////////////////////////////
// HANDLING POST REQUESTS - MULTIPART CONTENT //
////////////////////////////////////////////////

char	*Response::extractMultipartBoundary(size_t& boundarySize)
{
	size_t			ind1;
	std::string&	contentType = this->_request->_headers["Content-Type"];
	char*			boundary;

	ind1 = contentType.find("boundary=");
	if (ind1 == std::string::npos)
		return (NULL);
	boundarySize = contentType.size() - ind1 - 9 + 2; // +2 for leading '--'
	boundary = new char[boundarySize + 1]; // +1 for ending '\0'
	bzero(boundary, boundarySize + 1);
	boundary[0] = '-';
	boundary[1] = '-';
	strncpy(boundary + 2, contentType.data() + ind1 + 9, contentType.size() - ind1 - 9);
	std::cout << "\tidentified boundary : <" << boundary << ">\n";
	return (boundary);
}

int	Response::handlePostOnePart(std::string& dirPath, char* part, size_t partSize)
{
	ssize_t 	indBody;
	const char*	needle = "\r\n\r\n";
	size_t		ind1;
	size_t		ind2;
	std::string	partHeaders;
	std::string filename;
	std::string	fullPath;

	indBody = memFind(part, partSize, needle, 4);
	if (indBody == -1)
		return (this->makeErrorResponse("400 Bad Request (multipart part without linebreak)"), 1);
	indBody += 4;
	partHeaders = std::string(part, part + indBody);
	std::cout << "\thandling part with size " << partSize
		<< ", sep at index " << indBody << ", headers retrieved\n";
	ind1 = partHeaders.find("Content-Disposition:");
	if (ind1 == std::string::npos)
		return (this->makeErrorResponse("400 Bad Request (multipart part headers invalid)"), 1);
	ind2 = partHeaders.find("filename=\"", ind1);
	if (ind2 == std::string::npos)
		return (this->makeErrorResponse("400 Bad Request (multipart part without filename)"), 1);
	ind1 = partHeaders.find("\"", ind2 + 10);
	if (ind1 == std::string::npos || ind1 == ind2 + 1)
		return (this->makeErrorResponse("400 Bad Request (multipart part filename invalid)"), 1);
	filename = std::string(partHeaders.begin() + ind2 + 10, partHeaders.begin() + ind1);
	std::cout << "\t\t-> extracted filename : " << filename << "\n";
	fullPath = dirPath + filename;
	return (this->uploadFile(fullPath, partSize - indBody, part + indBody, 0));
}

void	Response::divideMultipart(std::string& dirPath, char* boundary, size_t boundarySize)
{
	char*			part;
	size_t			partSize;
	char*			body = this->_request->_body;
	size_t			bodySize = this->_request->_bodySize;
	ssize_t			ind1;
	ssize_t			ind2;

	ind1 = memFind(body, bodySize, boundary, boundarySize);
	if (ind1 == -1)
		return (this->makeErrorResponse("400 Bad Request (multipart body without boundary occurrence)"));
	while (ind1 != -1)
	{
		ind2 = memFind(body + ind1 + boundarySize, bodySize - ind1 - boundarySize, boundary, boundarySize);
		if (ind2 != -1)
		{
			ind2 = ind2 + ind1 + boundarySize;
			std::cout << "\tfound a part between indexes " << (ind1 + boundarySize) << " and " << ind2 << "\n";
			partSize = ind2 - ind1 - boundarySize;
			part = new char[partSize + 1];
			part[partSize] = '\0'; // #f : remove
			memcpy(part, body + ind1 + boundarySize, partSize);
			if (this->handlePostOnePart(dirPath, part, partSize))
			{
				delete[] part;
				return ;
			}
			delete[] part;
		}
		ind1 = ind2;
	}
	std::cout << "\thandled successfully all parts of multipart upload\n";
	this->makeSuccessResponse("200 OK");
}

int	Response::handlePostMultipart()
{
	char*			boundary;
	size_t			boundarySize;
	std::string		dirPath;

	std::cout << "\tchecks on POST request with multipart content\n";
	if (!(this->_request->_toDir))
		return (this->makeErrorResponse("403 Forbidden (multipart upload path with filename)"), 0);
	if (!(this->_location) || this->_location->uploadPath.empty())
		return (this->makeErrorResponse("403 Forbidden (upload not allowed by location)"), 0);
	if (this->pathToUpload(dirPath, 1))
		return (this->makeErrorResponse("404 Not Found (invalid upload path)"), 0);
	boundary = this->extractMultipartBoundary(boundarySize);
	if (!boundary)
		return (this->makeErrorResponse("400 Bad Request (no multipart boundary)"), 0);

	this->divideMultipart(dirPath, boundary, boundarySize);
	delete[] boundary;
	return (0);
}

/////////////////////////////////////////////
// HANDLING POST REQUESTS - SIMPLE CONTENT //
/////////////////////////////////////////////

int	Response::handlePostRaw()
{
	std::string	fullPath;

	std::cout << "\tchecks on POST request uploading raw file\n";
	if (this->_request->_toDir)
		return (this->makeErrorResponse("403 Forbidden (upload with no filename)"), 0);
	if (!(this->_location) || this->_location->uploadPath.empty())
		return (this->makeErrorResponse("403 Forbidden (upload not allowed by location)"), 0);
	if (this->pathToUpload(fullPath, 0))
		return (this->makeErrorResponse("404 Not Found (invalid upload path)"), 0);
	return (this->uploadFile(fullPath, this->_request->_bodySize, this->_request->_body, 1));
}

////////////////////////////
// HANDLING POST REQUESTS //
////////////////////////////

// Handles POST requests without CGI -> the request contains the content of a file
// that must be stored on the server side at the location given by the path
int	Response::handlePost()
{
	if (this->_request->_headers.count("Content-Type") == 0)
		return (this->makeErrorResponse("400 Bad Request (no content type)"), 0);
	if (this->_request->_headers["Content-Type"].compare(0, 19, "multipart/form-data") == 0)
		return (this->handlePostMultipart());
	else
		return (this->handlePostRaw());
}

//////////////////////////////
// HANDLING DELETE REQUESTS //
//////////////////////////////

int	Response::handleDelete()
{
	std::string	fullPath;
	bool		fileExists;

	std::cout << "\tchecks on DELETE\n";
	if (this->_request->_toDir)
		return (this->makeErrorResponse("403 Deleting Directory Forbidden"), 0);
	if (this->pathToFile(fullPath, 1, 0, NULL)) // #f : invalidated
		return (this->makeErrorResponse("404 Not Found"), 0);
	fileExists = (access(fullPath.c_str(), F_OK) == 0);
	std::cout << "\tchecks ended ; file exists : " << fileExists << "\n";
	if (!fileExists)
		return (this->makeErrorResponse("500 Internal Server Error"), 0);
	if (remove(fullPath.c_str()))
		return (this->makeErrorResponse("500 Internal Server Error"), 0);
	std::cout << "\tfile successfully deleted\n";
	this->makeSuccessResponse("500 Internal Server Error");
	return (0);
}
