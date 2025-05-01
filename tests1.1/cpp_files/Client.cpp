#include "../hpp_files/Webserv.hpp"

/////////////////////////////
// CANONICAL ORTHODOX FORM //
/////////////////////////////

Client::Client(int clientSocketFd)
{
	std::cout << "Parameter constructor for Client\n";
	this->_socketFd = clientSocketFd;
	this->_state = RECEIVING;
}

Client::~Client(void)
{
	std::cout << "Destructor for Client\n";
	close(this->_socketFd);
}

///////////////
// ACCESSORS //
///////////////

int	Client::getSocketFd()
{
	return (this->_socketFd);
}

////////////////////
// MAIN INTERFACE //
////////////////////

void	Client::preparePollFd(struct pollfd& pfd)
{
	pfd.fd = this->_socketFd;
	if (this->_state == RECEIVING)
		pfd.events = POLLIN;
	else if (this->_state == SENDING)
		pfd.events = POLLOUT;
}

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

int		Client::sendHTTP(void)
{
	unsigned int	nBytesWritten;

	std::cout << "\t[CH::sendHTTP] sending to client with socket at fd " << this->_socketFd << "\n";
	nBytesWritten = send(this->_socketFd, "coucou", 6, 0);
	if (nBytesWritten != 6)
		return (logError("\t[CH::receiveHTTP] error when writing to socket", 1));
	this->_state = RECEIVING;
	return (0);
}

// #f : check if there is still stuff to read, pass to SENDING only when fully read
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
	this->_state = SENDING;
	return (0);
}
