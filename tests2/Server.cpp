#include "Server.hpp"

/////////////////////////////
// CANONICAL ORTHODOX FORM //
/////////////////////////////

// Initializes the server internal variables as defined in configuration file
Server::Server(std::string cfgFileName)
{
	std::cout << "Parameter constructor for Server\n";
	(void)cfgFileName;
	this->_port = 8080;
}

Server::~Server(void)
{
	std::cout << "Destructor for Server\n";
	close(this->_mainSocketFd);
}

///////////////
// ACCESSORS //
///////////////

////////////////////
// MAIN INTERFACE //
////////////////////

// Calls three functions to setup the main socket of the server :
// 		\ <socket> : create the socket (represented by a file, with a fd as susual)
// 		\ <bind> : define on which IPaddress and port the socket listens
// 				   (using a structure provided by the library and which has to be cast in the function call)
// 		\ <listen> : have the socket start listening, ie. registering incoming connections in a queue
// 					 which can hold a queue of MAX_QUEUED_CONNECTIONS requests before refusing connections
// Details :
// 		\ defining the IPaddress to INADDR_ANY means accepting incoming connections
// 			received on any of our device's interfaces
// 				(since a device can have several interfaces, each with a different IP)
// 		\ functions <htonl> and <htons> are used to convers bit representations of address and port numbers
// 		  to big-endian/small-endian according to what convention the OS is using
// 		\ function <setsockopt> is used to allow webserv to bind its main socket to a port
//	 		even if there still is an active connection (from a previous webserv process) in TIME_WAIT using that port
int		Server::startServer(void)
{
	struct sockaddr_in	socketAddress;
	int					reuseaddrOptionValue = 1;

	this->_mainSocketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (this->_mainSocketFd < 0)
		return (logError("[startServer] error when creating main socket", 1));
	std::cout << "[startServer] main socket created with fd " << this->_mainSocketFd << "\n";

	setsockopt(this->_mainSocketFd, SOL_SOCKET, SO_REUSEADDR, &reuseaddrOptionValue, sizeof(reuseaddrOptionValue));
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons(this->_port);
	socketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(this->_mainSocketFd, reinterpret_cast<sockaddr*>(&socketAddress), sizeof(socketAddress)) < 0)
		return (logError("[startServer] error when binding socket to address", 1));
	std::cout << "[startServer] main socket bound to address (port : " << this->_port << ")\n";

	if (listen(this->_mainSocketFd, MAX_QUEUED_CONNECTIONS) < 0)
		return (logError("[startServer] error when passing main socket into 'listen' mode", 1));
	std::cout << "[startServer] main socket passed into 'listen' mode\n";
	return (0);	
}

int		Server::serverLoop(void)
{
	struct pollfd*	pfds;
	int				nfds;
	int				nfdsReady;

	std::cout << "Entering server mainloop\n";
	while (1)
	{
		std::cout << "------------\n";
		nfds = this->createPollFds(&pfds);
		std::cout << "Poll structures list created, calling <poll>\n";
		nfdsReady = poll(pfds, nfds, 20000);
		if (nfdsReady == -1)
			return (logError("[serverLoop] error during poll", 1));
		std::cout << "Number of sockets ready for I/O : " << nfdsReady << "\n";
		if (nfdsReady > 0 && this->goThroughEvents(pfds, nfds))
			return (1);
		this->removeClosedConnections();
		sleep(1);
	}
	return (0);
}
int		Server::goThroughEvents(struct pollfd* pfds, int nfds)
{
	int				clientInd;

	for (clientInd = 0; clientInd < nfds; clientInd++)
	{
		if (pfds[clientInd].revents != 0)
		{
			std::cout << "Socket fd " << pfds[clientInd].fd << " is ready, events : ";
			this->displayEvents(pfds[clientInd].revents);
			if (clientInd == nfds - 1 && handleNewConnection())
				return (1);
			if (clientInd < nfds - 1 && this->handleOldConnection(clientInd, pfds[clientInd]))
				return (1);
		}
	}
	return (0);
}

int		Server::handleOldConnection(int clientInd, struct pollfd& pfd)
{
	int	ret;

	ret = this->clientHandlers[clientInd]->handleEvent(pfd);
	if (ret == 1)
		return (1);
	if (ret == 2)
	{
		delete this->clientHandlers[clientInd];
		this->clientHandlers[clientInd] = NULL;
	}
	return (0);
}

// Handles new connections received by the (currently listening) main socket,
// 		ie. connections from clients that are not already connected to the server
// This function is only called if <poll> detected movement on the main socket,
// 		so it is certain that there are connections waiting in the main socket queue :
// 			the call to <accept> will select the first one, remove it from the queue,
// 			and create a new socket + an instance of Clienthandler dedicated to that client
// /!\ The IPaddress+port stored in <clientAddress> by function <accept> is the client address,
// 	   ie. the IP address of the client device and the port of this device on which the client receives packets :
// 			when we call function `send` to send data to client, this IPaddress+port will be used as destination of packet
// 			(but we do not need to keep it in memory ourselves, ii is managed by the OS)
int		Server::handleNewConnection(void)
{
	struct sockaddr_in	clientAddress;
	socklen_t			clientAddressSize;
	int					clientSocketFd;

	clientAddressSize = sizeof(clientAddress);
	clientSocketFd = accept(this->_mainSocketFd, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressSize);
	if (clientSocketFd < 0)
		return (logError("[handleNewConnection] error when trying to accept an incoming connection", 1));
	std::cout << "[waitForConnection] connection accepted in socket fd " << clientSocketFd
		<< " (IP " << inet_ntoa(clientAddress.sin_addr)
		<< " ; port " << ntohs(clientAddress.sin_port) << ")\n";
	this->clientHandlers.push_back(new ClientHandler(clientSocketFd));
	return (0);
}

///////////
// UTILS //
///////////

void	Server::removeClosedConnections(void)
{
	int	ind;

	std::cout << "Removing handlers of closed connections from client handlers array\n";
	for (ind = this->clientHandlers.size() - 1; ind >= 0; ind--)
	{
		if (this->clientHandlers[ind] == NULL)
		{
			std::cout << "\tremoving at index " << ind << "\n";
			this->clientHandlers.erase(this->clientHandlers.begin() + ind);
		}
	}
}

int		Server::createPollFds(struct pollfd **pfds)
{
	int	nfds = this->clientHandlers.size() + 1;
	int	ind;

	*pfds = new struct pollfd [nfds];
	for (ind = 0; ind < nfds - 1; ind++)
		this->clientHandlers[ind]->preparePollFd((*pfds)[ind]);
	(*pfds)[nfds - 1].fd = this->_mainSocketFd;
	(*pfds)[nfds - 1].events = POLLIN;
	
	std::cout << "Currently maintaining " << nfds - 1 << " client sockets, + 1 main socket\n";
	for (ind = 0; ind < nfds - 1; ind++)
	{
		std::cout << "\tclient socket at fd " << (*pfds)[ind].fd << " awaits events : ";
		this->displayEvents((*pfds)[ind].events);
	}
	std::cout << "\tmain socket at fd " << (*pfds)[nfds - 1].fd << " awaits events : ";
	this->displayEvents((*pfds)[nfds - 1].events);
	return (nfds);
}


void	Server::displayEvents(short revents)
{
	if (revents & POLLIN)
		std::cout << "POLLIN ";
	if (revents & POLLOUT)
		std::cout << "POLLOUT ";
	if (revents & POLLHUP)
		std::cout << "POLLHUP ";
	if (revents & POLLERR)
		std::cout << "POLLERR ";
	if (revents & POLLNVAL)
		std::cout << "POLLNVAL ";
	std::cout << "\n";
}

