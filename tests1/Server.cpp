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

// Handles connections received by the (currently listening) main socket
// by selecting one of the connections and creating a new socket for it at fd <clientSocketFd>
// 		\ if connections were already received, they are stored in the socket's queue,
// 		  so the call to <accept> will select the first one and remove it from the queue
// 		\ if there are no pending connections (the queue is empty),
// 		  the call to <accept> will be BLOCKING, waiting for a connection to be received
// then sends the client socket of the new connection to a function that will handle the client's request
// /!\ The IPaddress+port stored in <clientAddress> by function <accept> is the client address,
// 	   ie. the IP address of the client device and the port of this device on which the client receives packets :
// 			when we call function `send` to send data to client, this IPaddress+port will be used as destination of packet
// 			(but we do not need to keep it in memory ourselves, ii is managed by the OS)
int		Server::waitForConnection(void)
{
	struct sockaddr_in	clientAddress;
	socklen_t			clientAddressSize;
	int					clientSocketFd;

	clientAddressSize = sizeof(clientAddress);
	clientSocketFd = accept(this->_mainSocketFd, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressSize);
	if (clientSocketFd < 0)
		return (logError("[waitForConnection] error when trying to accept an incoming connection", 1));
	std::cout << "[waitForConnection] connection accepted in socket fd " << clientSocketFd
		<< " (IP " << inet_ntoa(clientAddress.sin_addr)
		<< " ; port " << ntohs(clientAddress.sin_port) << ")\n";
	
	return (this->handleClientRequest(clientSocketFd));
}

int		Server::handleClientRequest(int clientSocketFd)
{
	char			buffer[1024];
	unsigned int	nBytesRead;

	nBytesRead = read(clientSocketFd, buffer, 1024);
	if (nBytesRead < 0)
		return (logError("[handleClientRequest] failed to read from client socket", 1));
	std::cout << "[handleClientRequest] Content read from client socket " << clientSocketFd
		<< " :\n----------\n" << std::string(buffer) << "\n----------\n";
	close(clientSocketFd);
	return (0);
}

