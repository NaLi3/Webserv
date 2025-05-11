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
	this->_clientPortAddr = clientPortaddr;
	this->_response = NULL;
	this->_request = NULL;
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

// Fills the <poll> struct with the expected event information
// 		\ fd of the socket dedicated to this client
// 		\ event flags depending on whether this client must send a request,
// 			or data must be sent to this client
void	Client::preparePollFd(struct pollfd& pfd)
{
	pfd.fd = this->_socketFd;
	if (this->_state == RECEIVING)
		pfd.events = POLLIN;
	else if (this->_state == SENDING)
		pfd.events = POLLOUT;
}

// Redirects an event received on the socket of this client to the correct method
// (apparently event POLLHUP is never received for sockets)
int		Client::handleEvent(struct pollfd& pfd)
{
	if ((pfd.revents & POLLIN) && this->_state == RECEIVING)
		return (this->receiveHTTP());
	if ((pfd.revents & POLLOUT) && this->_state == SENDING)
		return (this->sendHTTP());
	if (pfd.revents & POLLHUP && this->_state != RECEIVING)
	{
		std::cout << "\t\t[CH::handleEvent] hangup from client with socket at fd " << this->_socketFd << "\n";
		return (2);
	}
	return (0);
}

///////////////////////
// SENDING TO SOCKET //
///////////////////////

// Uses function <send> to send the data in <_response> through the socket
// The socket may be too small to accept the full <_response>,
// 		so the progress in number of bytes sent is registered in <_nBytesSent>
// 		in order to send the next part of <_response> at the next call of <sendHTTP>
// -> state is changed from "SENDING" to "RECEIVING" only when the full <_response> was sent
// /!\ TCP connections are two-way, like 2 pipes (client->server) and (server->client)
// 	   so this function can send response bytes even while request bytes from the client are still in socket
int		Client::sendHTTP(void)
{
	long	nBytesWritten;

	std::cout << "\t\t[CH::sendHTTP] sending to client with socket at fd " << this->_socketFd
		<< " ; bytes sending progress : " << this->_nbytesSent << " / " << this->_response->getResponseSize() << "\n";
	nBytesWritten = send(this->_socketFd, this->_responseBuffer + this->_nbytesSent,
											this->_response->getResponseSize() - this->_nbytesSent, 0);
	if (nBytesWritten <= 0)
		return (logError("[CH::receiveHTTP] error when writing to socket", 1));
	std::cout << "\t\t[CH::sendHTTP] sent " << nBytesWritten << " bytes\n";
	this->_nbytesSent += nBytesWritten;
	if (this->_nbytesSent >= this->_response->getResponseSize())
	{
		std::cout << "\t\t[CH::sendHTTP] full response sent\n";
		if (this->_request)
		{
			delete this->_request;
			this->_request = NULL;
		}
		delete this->_response; this->_response = NULL;
		this->_requestHeaders = "";
		bzero(this->_lastBuffer, READSOCK_BUFFER_SIZE + 1);
		this->_state = RECEIVING;
	}
	return (0);
}

///////////////////////////
// RECEIVING FROM SOCKET //
///////////////////////////

// When a request in instance of <Request> is complete :
// 		\ create Instance of <Response> to generate response
// 		\ go into SENDING mode to await POLLOUT events on the socket to be able to send the response
// #f : put log in Request and in Response
int	Client::handleCompleteRequest()
{
	int		responseRet;

	this->_request->logRequest();
	this->_response = new Response(this->_request, this->_clientPortAddr,
				this->_vservers, this->_mainSocket.vservIndexes);
	responseRet = this->_response->produceResponse();
	//this->_response->logResponseBuffer();
	if (responseRet)
		return (1);
	this->_responseBuffer = this->_response->getResponseBuffer();
	this->_state = SENDING;
	this->_nbytesSent = 0;
	std::cout << "[CH::receiveHTTP] Going into sending mode\n";
	return (0);
}

// Creates a <Response> instance with minimal initialisation
// since it only has to generate the HTTP response for error 400 "Malformed request"
// then puts the <Client> instance into sending mode
void	Client::handleMalformedRequest()
{
	this->_response = new Response(400);
	this->_responseBuffer = this->_response->getResponseBuffer();
	this->_state = SENDING;
	this->_nbytesSent = 0;
}

int	Client::finishRequestHeaders(char *buffer, size_t bufferSize, int indHeadersEnd)
{
	char	bufferRemaining[READSOCK_BUFFER_SIZE + 1];

	this->_requestHeaders.append(buffer, indHeadersEnd + 1);
	std::cout << "\t\t-> full request headers :\n\033[31m<\n" << this->_requestHeaders << "\n>\033[0m\n";
	this->_request = new Request(this->_mainSocket);
	if (this->_request->parse(this->_requestHeaders))
	{
		std::cout << "\t\tMalformed request : headers cannot be parsed\n";
		this->handleMalformedRequest();
		return (0);
	}
	if (this->_request->_hasBody)
	{
		this->_request->allocateBody();
		if (indHeadersEnd == static_cast<int>(bufferSize - 1))
			return (0);
		std::cout << "\t\trequest has body of expected size " << this->_request->_bodySize
			<< ", appending remaining bytes of buffer to body\n";
		bzero(bufferRemaining, READSOCK_BUFFER_SIZE + 1);
		memcpy(bufferRemaining, buffer + indHeadersEnd + 1, bufferSize - indHeadersEnd - 1);
		this->receiveBodyPart(bufferRemaining, bufferSize - indHeadersEnd - 1);
		return (0);
	}
	else
		return (this->handleCompleteRequest());
}

//#f : check if its the problem that headers part contains final '\r\n'
int	Client::receiveHeadersPart(char *buffer, size_t bufferSize)
{
	int		indHeadersEnd;

	indHeadersEnd = Request::containsHeadersEnd(this->_lastBuffer, buffer, bufferSize);
	std::cout << "\t\tResult of search for headers end in bytes read : " << indHeadersEnd << "\n";
	if (indHeadersEnd == -2)
	{
		std::cout << "\t\tMalformed request : EOS character in headers part\n";
		this->handleMalformedRequest();
		return (0);
	}
	else if (indHeadersEnd == -1)
	{
		this->_requestHeaders.append(buffer, bufferSize);
		bzero(this->_lastBuffer, READSOCK_BUFFER_SIZE + 1);
		strncpy(this->_lastBuffer, buffer, bufferSize);
		std::cout << "\t\t-> appended bytes to request headers :\n\033[31m<\n" << buffer << "\n>\033[0m\n";
		return (0);
	}
	else
		return (this->finishRequestHeaders(buffer, bufferSize, indHeadersEnd));
}

int	Client::receiveBodyPart(char* buffer, size_t bufferSize)
{
	if (this->_request->_nReceivedBodyBytes + bufferSize == this->_request->_bodySize)
	{
		std::cout << "\t\t[CH::receiveHTTP] Received enough bytes to complete body ("
			<< this->_request->_bodySize << " / " << this->_request->_bodySize << ")\n";
		this->_request->appendToBody(buffer, bufferSize);
		return (this->handleCompleteRequest());
	}
	else if (this->_request->_nReceivedBodyBytes + bufferSize > this->_request->_bodySize)
	{
		std::cout << "\t\t[CH::receiveHTTP] (Warning) Received more than enough bytes received for body\n";
		this->_request->appendToBody(buffer, this->_request->_bodySize - this->_request->_nReceivedBodyBytes);
		return (this->handleCompleteRequest());
	}
	else
	{
		std::cout << "\t\t[CH::receiveHTTP] Received bytes for body, still not complete ("
			<< this->_request->_nReceivedBodyBytes + bufferSize << " / " << this->_request->_bodySize << ")\n";
		this->_request->appendToBody(buffer, bufferSize);
	}
	return (0);
}

// On POLLIN event, reads bytes from the socket, adds them to the <_requestBuffer>,
// then checks if the request in <_requestBuffer> is complete and must be answered
// 		(if not, this instance will remain in RECEIVING mode awaiting further POLLIN)
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
	//for (unsigned int ind = nBytesRead - 4; ind < nBytesRead; ind++) // #f : remove this
	//	std::cout << "\t\t\tind " << ind << " : char " << static_cast<int>(buffer[ind]) << "\n";

	if (!(this->_request))
		return (this->receiveHeadersPart(buffer, nBytesRead));
	else
		return (this->receiveBodyPart(buffer, nBytesRead));
}
