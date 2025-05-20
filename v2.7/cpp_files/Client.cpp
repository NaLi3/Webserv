#include "../hpp_files/webserv.hpp"

/////////////////////////////
// CANONICAL ORTHODOX FORM //
/////////////////////////////

// Parameters to constructor :
// 		\ <clientSocketFd> is the fd of the socket connecting the server to this instance
// 		\ <mainSocket> is (a reference to) the structure representing the main socket
// 		  where this client first reached the server and was accepted
// 			-> necessary because it holds in field <vservIndexes> the indexes of the virtual servers
// 			   listening on a list of portaddr which must include <portaddr>
// 					<=> the indexes of the virtual servers to which the client has access
// 		\ <vservers> is (a reference to) a list of structures representing the virtual servers
// 			(the indexes in <mainSocket.vservIndexes> are positions in this list)
Client::Client(int socketFd, t_portaddr clientPortaddr,
	t_mainSocket& mainSocket, std::vector<t_vserver>& vservers) :
	_socketFd(socketFd), _mainSocket(mainSocket), _vservers(vservers)
{
	std::cout << "Parameter constructor for ClientHandler\n";
	std::cout << "\t(dedicated socket has fd " << this->_socketFd << " ; main socket had fd "
		<< this->_mainSocket.fd << " listening on " << this->_mainSocket.portaddr << ")\n";
	this->_state = RECEIVING;
	this->_closeAfterSent = 0;
	this->_clientPortAddr = clientPortaddr;
	this->_response = NULL;
	this->_request = NULL;
	this->_recurrLvl = 0;
}

// Destructor closes the dedicated socket connected to the client
Client::~Client(void)
{
	std::cout << "Destructor for Client\n";
	if (this->_request)
		delete this->_request;
	if (this->_response)
		delete this->_response;
	close(this->_socketFd);
}

///////////////
// ACCESSORS //
///////////////

int	Client::getSocketFd() const
{
	return (this->_socketFd);
}

t_mainSocket&	Client::getMainSocket(void)
{
	return (this->_mainSocket);
}

////////////////////
// MAIN INTERFACE //
////////////////////

// When the client instance is waiting for a CGI script to terminate
// and receives a POLLIN/POLLHUP event on the CGI output pipe fd,
// calls <Response.checkCGIFinished> to read what the CGI outputted
// and processes its return value exactly like in <handleCompleteRequest>
// 		\ 3 means CGI child still alive, continue to wait
// 		\ other codes mean CGI child terminated (detected by <read> call returning 0 EOF)
// If the CGI script terminated, state can change to SENDING
int	Client::updateWaitingCGI()
{
	int	responseRet;

	std::cout << "[CH::updWaitCGI] instance waiting for CGI -> updating state\n";
	responseRet = this->_response->checkCGIFinished();
	if (responseRet == 1)
		return (1);
	else if (responseRet == 2)
	{
		delete this->_response; this->_response = NULL;
		if (this->_recurrLvl > 100)
			return (this->handleProblematicRequest("508 Loop Detected", 0), 0);
		std::cout << "[CH::updWaitCGI] Response instance returned local redirect, recurring\n";
		this->_recurrLvl++;
		return (this->handleCompleteRequest());
	}
	else if (responseRet == 3)
	{
		std::cout << "\t[CH::updWaitCGI] ... still waiting, no state change\n";
		return (0);
	}
	std::cout << "\t[CH::updWaitCGI] ... CGI did terminate, pass to sending\n";
	this->passToSending();
	return (0);
}

// Fills the <poll> struct with the expected event information
// 		\ fd of the socket dedicated to this client
// 		\ event flags depending on whether this client must send a request,
// 			or data must be sent to this client
// Special case : <Client> instance may be waiting for a CGI script to terminate
// 	(with a hanging <Response> instance waiting to process CGI output),
// in this case it the information is instead :
// 		\ fd of the receiving end of the CGI output pipe
// 		\ event flags always POLLIN (some CGI output is expected on that fd)
void	Client::preparePollFd(struct pollfd& pfd)
{
	if (this->_state == WAITING_CGI)
	{
		pfd.fd = this->_response->getCGIChildFd();
		pfd.events = POLLIN;
	}
	else
	{
		pfd.fd = this->_socketFd;
		if (this->_state == RECEIVING)
			pfd.events = POLLIN;
		else if (this->_state == SENDING)
			pfd.events = POLLOUT;
	}
}

// Redirects an event received on the socket of this client to the correct method
// (apparently event POLLHUP is never received for sockets)
// Return value may be 0 for success, or
// 		\ 1 : fatal error encountered either by <Client> (recv/sendv)
// 			or by its instance of <Response> (should never happen, see <handleCompleteRequest>)
// 		\ 2 : unable to continue the connection, because either
// 			~ the client on the other side of the TCP connection hung up
// 				<=> (POLLHUP event, or POLLIN event while nothing to read)
// 			~ the received request bytes are malformed in such a way
// 			  that it compromises the parsing of further requests
// 				(notably if headers are unreadable -> <Client> cannot know if there is a body)
// Special case : <Client> instance may be waiting for a CGI script to terminate,
// 		if so the fd given to <poll> was a pipe receiving end fd and should receive only POLLIN/POLLHUP
int		Client::handleEvent(struct pollfd& pfd)
{
	if (this->_state == WAITING_CGI)
	{
		if (pfd.revents & POLLIN || pfd.revents & POLLHUP)
			return (this->updateWaitingCGI());
		else
			return (1);
	}
	if ((pfd.revents & POLLIN) && this->_state == RECEIVING)
		return (this->receiveHTTP());
	if ((pfd.revents & POLLOUT) && this->_state == SENDING)
		return (this->sendHTTP());
	if (((pfd.revents & POLLHUP) || (pfd.events & POLLERR)))
	{
		std::cout << "\t\t[CH::handleEvent] hangup from client with socket at fd " << this->_socketFd << "\n";
		return (2);
	}
	return (0);
}

///////////////////////
// SENDING TO SOCKET //
///////////////////////

// Passes this instance of client into RECEIVING mode (listens to incoming requests bytes)
void	Client::passToReceiving()
{
	if (this->_request)
	{
		delete this->_request;
		this->_request = NULL;
	}
	delete this->_response; this->_response = NULL;
	this->_requestHeaders = "";
	this->_state = RECEIVING;
	this->_recurrLvl = 0;
	std::cout << "[CH::state] Going into receiving mode\n";
}

// Uses function <send> to send the data in <_responseBuffer> through the socket ;
// 		if all data is sent then puts this instance of <Client> back in RECEIVING mode
// 		(by emptying the buffers and destroying the instances of <Response> and <Request>,
// 			which frees all memory that those instances allocated)
// Returns 0 for success, 1 in case of <send> error, 2 if <_closeAfterSent> is true
// 		(case where an error response had to be sent to the client before closing the connection)
// The socket may be too small to accept the full <_responseBuffer> in one <send>,
// 		so the progress in number of bytes sent is registered in <_nBytesSent>
// 			-> the next bytes in <_responseBuffer> wait for the next call to <sendHTTP> to be sent
// /!\ TCP connections are two-way, like 2 pipes (client->server) and (server->client)
// 	   so this function can send response bytes even while request bytes from the client are still in socket
int		Client::sendHTTP(void)
{
	ssize_t	nBytesWritten;
	size_t	nBytesToWrite;

	std::cout << "\t\t[CH::sendHTTP] sending to client with socket at fd " << this->_socketFd
		<< " ; bytes sending progress : " << this->_nbytesSent << " / " << this->_responseSize << "\n";
	nBytesToWrite = std::min(WRITESOCK_BUFFER_SIZE, this->_responseSize - this->_nbytesSent);
	nBytesWritten = send(this->_socketFd, this->_responseBuffer + this->_nbytesSent, nBytesToWrite, 0);
	if (nBytesWritten <= 0)
		return (logError("[CH::receiveHTTP] error when writing to socket", 1));
	std::cout << "\t\t[CH::sendHTTP] sent " << nBytesWritten << " bytes (tried " << nBytesToWrite << ")\n";
	this->_nbytesSent += nBytesWritten;
	if (this->_nbytesSent >= this->_responseSize)
	{
		std::cout << "\t\t[CH::sendHTTP] full response sent\n";
		if (this->_closeAfterSent)
			return (logError("[CH::sendHTTP] closing after having sent error response", 0), 2);
		else
			this->passToReceiving();
	}
	return (0);
}

///////////////////////////
// RECEIVING FROM SOCKET //
///////////////////////////

// Creates a <Response> instance with minimal initialisation
// since it only has to generate the HTTP response for an error (400/500)
// encountered by this instance of <Client>
// Then puts this instance of <Client> into SENDING mode,
// and, if <closeConnection> is 1, sets <_closeAfterSent> so that after the response is sent,
// the connection is closed instead of going back to RECEIVING mode
// 		(<=> renounce to persistence of connection because error compromises future handling of requests)
void	Client::handleProblematicRequest(std::string status, int closeConnection)
{
	this->_response = new Response(status);
	if (closeConnection)
		this->_closeAfterSent = 1;
	this->passToSending();
}

// Passes this instance of client into SENDING mode (tries to send response bytes)
void	Client::passToSending()
{
	this->_responseBuffer = this->_response->getResponseBuffer();
	this->_responseSize = this->_response->getResponseSize();
	this->_state = SENDING;
	this->_nbytesSent = 0;
	this->_recurrLvl = 0;
	std::cout << "[CH::state] Going into sending mode\n";
}

// When a request in instance of <Request> is complete :
// 		\ create Instance of <Response> to generate response
// 		\ checks Response returm value : 0 for success, 1 for fatal error, and specialized code :
// 			~ 2 for LOCAL redirection (happens for CGI) <=> the instance of <Request> was modified
// 			  with a new path, so a new instance of <Response> must be created to handle it
// 				(-> counter <recurrLvl> to avoid infinite redirections)
// 			~ 3 for CGI script launched but not finished : this instance of <Client> goes into WAITING mode
// 			  so that, at each <preparePollFd> call, it asks the <Response> instance if CGI script finished 
// 		\ puts this instance of <Client> into SENDING mode,
// 			so that it awaits a POLLOUT event on the socket to be able to send the response
// 				(this also stores the response size and a pointer to the response buffer in member variables)
// /!\ returns 1 in case of fatal error encountered in <Response.produceResponse()>,
// but this never happens in current implementation : errors when producing response are either
// 		\ client errors (invalid path, forbidden method) -> generates response with code 400
// 		\ system calls errors of <open>/<read>/<pipe>... -> generates response with code 500
// <=> even with system call errors, the server and this connection are maintained running,
// 	   to respect the principle "server should remain available at all times"
int	Client::handleCompleteRequest()
{
	int		responseRet;

	this->_request->logRequest();
	std::cout << "[CH::handleCompReq] Creating Response instance for producing response\n";
	this->_response = new Response(this->_request, this->_clientPortAddr);
	responseRet = this->_response->produceResponse();
	//this->_response->logResponseBuffer(); // activate when only non-binary files are transmitted
	if (responseRet == 1) // never happens in current implementation
	{
		std::cout << " -> Response instance encountered fatal error\n";
		return (1);
	}
	else if (responseRet == 2)
	{
		delete this->_response; this->_response = NULL;
		if (this->_recurrLvl > 100)
			return (this->handleProblematicRequest("508 Loop Detected", 0), 0);
		std::cout << " -> Response instance returned local redirect, recurring\n";
		this->_recurrLvl++;
		return (this->handleCompleteRequest());
	}
	else if (responseRet == 3)
	{
		std::cout << " -> Response instance required wait for CGI\n";
		this->_state = WAITING_CGI;
		return (0);
	}
	std::cout << " -> Response instance created HTTP response\n";
	this->passToSending();
	return (0);
}

// Called when bytes in <buffer> received on the socket contain request separator '\r\n\r\n'
// (at index <indHeadersEnd>, the index of the last '\n' in '\r\n\r\n')
// 		\ appends bytes BEFORE '\r\n\r\n' to string <_requestHeaders>
// 		\ creates a new <Request> instance and lets it parse <_requestHeaders>,
// 			checking for errors (malformed requests)
// 				+ lets it determine the targeted virtual server and location
// 		\ checks that body size is not too large for virtual server and location
// 		\ if request has a body, lets instance of <Request> allocate a body buffer,
// 			then adds bytes AFTER '\r\n\r\n' to this body
// 		\ if request has no body, calls <handleCompleteRequest> straight to pass into SENDING mode
// Some details :
// 		\ <bufferRemaining> is built to pass the bytes AFTER '\r\n\r\n' to <receiveBodyPart>
// 		\ <receiveBodyPart> may call <handleCompleteRequest> if the remaining bytes are enough to complete body
// 			(edge case : if body size <=> "Content-Length" is 0,
// 				then  <receiveBodyPart> will definitely call <handleCompleteRequest>)
// 		\ condition `indHeadersEnd == bufferSize - 1 && bodySize > 0` handles cases where separator
// 		  is at the end of buffer, so there is nothing left in buffer to append to body
// 			(except in the edge case where body size is 0, then <receiveBodyPart> must be called to complete request)
// 		\ in case of unreadable headers (<Request.parse> returns 1), the connection must be closed
// 		  after the response '400' has been sent. since now the server is desynchronized from client :
// 			header "Content-Length" is unknown, so there is no way to know where the body ends
int	Client::finishRequestHeaders(char *buffer, size_t bufferSize, ssize_t indHeadersEnd)
{
	char	bufferRemaining[READSOCK_BUFFER_SIZE + 1];

	this->_requestHeaders.append(buffer, indHeadersEnd + 1);
	std::cout << "\t\t-> full request headers :\n\033[31m<\n" << this->_requestHeaders << "\n>\033[0m\n";
	this->_request = new Request(this->_mainSocket);
	if (this->_request->parse(this->_requestHeaders))
	{
		std::cout << "\t\tMalformed request : headers cannot be parsed\n";
		this->handleProblematicRequest("400 Bad Request", 1);
		return (0);
	}
	this->_request->identifyTargeted(this->_vservers, this->_mainSocket.vservIndexes);
	if (this->_request->bodySizeForbidden())
		return (this->handleProblematicRequest("413 Content Too Large", 1), 0);
	if (this->_request->methodForbidden())
		return (this->handleProblematicRequest("405 Method Forbidden", 1), 0);
	if (this->_request->_hasBody)
	{
		this->_request->allocateBody();
		if (indHeadersEnd == static_cast<ssize_t>(bufferSize - 1) && this->_request->_bodySize > 0)
			return (0);
		std::cout << "\t\trequest has body of expected size " << this->_request->_bodySize
			<< ", appending remaining bytes of buffer to body\n";
		bzero(bufferRemaining, READSOCK_BUFFER_SIZE + 1);
		memcpy(bufferRemaining, buffer + indHeadersEnd + 1, bufferSize - indHeadersEnd - 1);
		this->receiveBodyPart(bufferRemaining, bufferSize - indHeadersEnd - 1); // second argument may be 0
		return (0);
	}
	else
		return (this->handleCompleteRequest());
}

// Handles bytes in <buffer> received on the socket BEFORE the request separator '\r\n\r\n'
// Searches the bytes for '\r\n\r\n', then
// 		\ if found, request headers part is finished
// 			so instance of <Request> can be created and headers can be parsed
// 		\ if not found, bytes are stored in <_requestHeaders>
// 			and this instance of <Client> will wait for other POLLIN on socket
// Some details :
// 		\ an HTTP request containing byte '\0' before '\r\n\r\n' is malformed,
// 			since the headers part is simple text where '\0' is EOS
// 		\ <Request.containsHeadersEnd> returns exactly the index of the last '\n' in '\r\n\r\n'
// 		\ since request bytes are read part by part, separator '\r\n\r\n' may be split on multiple parts,
// 			so we have to provide the last part AND the previously received bytes
// 			to <Request.containsHeadersEnd>, which will assemble them and searche the whole
int	Client::receiveHeadersPart(char *buffer, size_t bufferSize)
{
	ssize_t		indHeadersEnd;

	indHeadersEnd = Request::containsHeadersEnd(this->_requestHeaders, buffer, bufferSize);
	std::cout << "\t\tResult of search for headers end in bytes read : " << indHeadersEnd << "\n";
	if (indHeadersEnd == -1 && strlen(buffer) != bufferSize)
	{
		std::cout << "\t\tError during search : EOS found in headers\n";
		this->handleProblematicRequest("500 Internal Server Error", 1);
		return (0);
	}
	else if (indHeadersEnd == -1)
	{
		this->_requestHeaders.append(buffer, bufferSize);
		std::cout << "\t\t-> appended bytes to request headers :\n\033[31m<\n" << buffer << "\n>\033[0m\n";
		return (0);
	}
	else
		return (this->finishRequestHeaders(buffer, bufferSize, indHeadersEnd));
}

// Handles bytes in <buffer> received on the socket AFTER the request separator '\r\n\r\n'
// 		<=> bytes belong to request body, so they are passed to <this->_request>
// If enough bytes were received to complete the body, <handleCompleteRequest> called,
// 		otherwise this instance of <Client> will simply wait for other POLLIN on socket
// <Request.appendToBody>'s return values are
// 		\ 0 for "body unfinished",
// 		\ 1 for "all body received", 2 for "... but more bytes received than expected"
// 		\ 3 for an error on chunked body, 4 for specific error "body size too large"
int	Client::receiveBodyPart(char* buffer, size_t bufferSize)
{
	int		ret;

	if (bufferSize == 0)
		return (this->_request->_bodySize == 0);
	std::cout << "\t\t[CH::receiveHTTP] Appending " << bufferSize << " bytes to body\n";
	ret = this->_request->appendToBody(buffer, bufferSize);
	if (ret == 4)
		return (this->handleProblematicRequest("413 Content Too Large", 1), 0);
	if (ret == 3)
		return (this->handleProblematicRequest("400 Bad Request (incoherent chunks)", 1), 0);
	else if (ret == 1 || ret == 2)
		return (this->handleCompleteRequest());
	else
		return (0);
}

// On POLLIN event, reads bytes from the socket into a <buffer>,
// then handles the buffer according to internal state
// 		\ if <_request> exists : bytes are body bytes to append
// 			(if those bytes complete the body, <receiveBodyPart> will change state to SENDING)
// 		\ if no <_request> yet : bytes contain seme HTTP headers to store and search
// 			(if those bytes contain request separator '\r\n\r\n',
// 				<_request> will be created and the headers will be parsed)
// UNIX API for tcp connections defines that end of connection with a client
// is signified by raising a POLLIN event for an empty socket
// 		so if the <read> call reads 0 bytes, <receiveHTTP> returns 2 to tell the server
// 		that this connections has ended and this instance of <Client> can be deleted
int		Client::receiveHTTP(void)
{
	char			buffer[READSOCK_BUFFER_SIZE + 1];
	ssize_t			recvRet;
	size_t			nBytesRead;

	std::cout << "\t\t[CH::receiveHTTP] reading from client with socket at fd " << this->_socketFd << "\n";
	bzero(buffer, READSOCK_BUFFER_SIZE + 1);
	recvRet = recv(this->_socketFd, buffer, READSOCK_BUFFER_SIZE, 0);
	if (recvRet < 0)
		return (logError("[CH::receiveHTTP] error when reading from socket", 1));
	nBytesRead = static_cast<size_t>(recvRet);
	if (nBytesRead == 0)
	{
		std::cout << "\t\t[CH::receiveHTTP] POLLIN while nothing to read from socket -> end of connection\n";
		return (2);
	}
	std::cout << "\t\t[CH::receiveHTTP] Read from socket : " << nBytesRead << " bytes\n";
	if (!(this->_request))
		return (this->receiveHeadersPart(buffer, nBytesRead));
	else
		return (this->receiveBodyPart(buffer, nBytesRead));
}
