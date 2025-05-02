#include "../hpp_files/webserv.hpp"

/////////////////////////////
// CANONICAL ORTHODOX FORM //
/////////////////////////////


// Parameters to constructor :
// 		\ <clientSocketFd> is the fd of the socket connecting the server to this client
// 		\ <mainSocket> is (a reference to) the structure representing the main socket
// 		  where this client first reached the server and was accepted
// 			-> necessary because it holds in field <portaddr> the portaddr used for connection
// 			   and in field <vservIndexes> the indexes of the virtual servers listening on that portaddr
// 					<=> the indexes of the virtual servers to which the client has access
// 		\ <vservers> is (a reference to) a list of structures representing the virtual servers
// 			(the indexes in <mainSocket.vservIndexes> are positions in this list)
Client::Client(int socketFd, t_mainSocket& mainSocket, std::vector<t_vserver>& vservers) :
	_socketFd(socketFd), _mainSocket(mainSocket), _vservers(vservers)
{
	std::cout << "Parameter constructor for ClientHandler\n";
	std::cout << "\tdedicated socket has fd " << this->_socketFd << " ; main socket had fd "
		<< this->_mainSocket.fd << " listening on " << this->_mainSocket.portaddr << "\n";
	this->_state = RECEIVING;
	this->_response_sent = false;
}

Client::~Client(void)
{
	std::cout << "Destructor for Client\n";
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
		std::cout << "\t[CH::handleEvent] hangup from client with socket at fd " << this->_socketFd << "\n";
		return (2);
	}
	return (0);
}

// Uses function <send> to send the data in <_response> through the socket
// The socket may be too small to accept the full <_response>,
// 		so the progress in number of bytes sent is registered in <_nBytesSent>
// 		in order to send the next part of <_response> at the next call of <sendHTTP>
// -> state is changed from "SENDING" to "RECEIVING" only when the full <_response> was sent
int		Client::sendHTTP(void)
{
	unsigned int	nBytesWritten;

	std::cout << "\t[CH::sendHTTP] sending to client with socket at fd " << this->_socketFd
		<< " ; bytes sending progress : " << this->_nbytesSent_tmp << " / " << this->_response_buffer.size() << "\n";
	nBytesWritten = send(this->_socketFd, this->_response_buffer.c_str() + this->_nbytesSent_tmp,
											this->_response_buffer.size() - this->_nbytesSent_tmp, 0);
	if (nBytesWritten <= 0)
		return (logError("\t[CH::receiveHTTP] error when writing to socket", 1));
	std::cout << "\t[CH::sendHTTP] sent " << nBytesWritten << " bytes\n";
	this->_nbytesSent_tmp += nBytesWritten;
	if (this->_nbytesSent_tmp >= this->_response_buffer.size())
	{
		std::cout << "\t[CH::sendHTTP] full response sent\n";
		this->_state = RECEIVING;
		this->_response_sent = 1;
	}
	return (0);
}

int		Client::receiveHTTP(void)
{
	char			buffer[1024];
	unsigned int	nBytesRead;

	std::cout << "\t[CH::receiveHTTP] reading from client with socket at fd " << this->_socketFd << "\n";
	bzero(buffer, 1024);
	nBytesRead = recv(this->_socketFd, buffer, 1024, 0);
	if (nBytesRead < 0)
		return (logError("\t[CH::receiveHTTP] error when reading from socket", 1));
	if (nBytesRead == 0)
	{
		std::cout << "\t[CH::receiveHTTP] POLLIN while nothing to read from socket -> end of connection\n";
		return (2);
	}
	std::cout << "\t[CH::receiveHTTP] Content read from socket"
		<< " :\n\033[32m< HTTP ---\n" << std::string(buffer) << "\n--- HTTP >\033[0m\n";
	this->_request_buffer.append(buffer, nBytesRead);
	if (this->_request.isComplete(this->_request_buffer))
	{
		if (!this->_request.parse(this->_request_buffer))
		{
			std::cout << "Bad Request" << std::endl;
			// this->_request.log_request();
			return (400);
		}
		else
		{
			this->_response_buffer = "Placeholder response builder";
		}
		this->_request.log_request();
		
		this->_state = SENDING;
		this->_response_sent = false;
		this->_nbytesSent_tmp = 0;
	}
	return (0);
}
