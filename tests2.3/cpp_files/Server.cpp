#include "../hpp_files/webserv.hpp"

/////////////////////////////
// CANONICAL ORTHODOX FORM //
/////////////////////////////

Server::Server()
{
	std::cout << "Parameter constructor for Server\n\n";
}

// Initializes the server internal variables as defined in configuration file,
// then determines which main sockets will have to be maintained
// 		/!\ structures representing main sockets are built, but no real socket is created
int		Server::initServer(std::string cfgFileName)
{
	g_global_signal = 0;
	if (std::signal(SIGINT, &handleSigint) == SIG_ERR)
		return (1);
	if (parseCfgFile(cfgFileName, this->_vservers))
		return (1);
	if (this->_vservers.size() == 0)
		return (1);
	logWebservConf(this->_vservers);
	this->initMainSockets();
	this->logMainSockets();
	return (0);
}

Server::~Server(void)
{
	std::cout << "Destructor for Server\n";
}

///////////////
// ACCESSORS //
///////////////

////////////////////
// INITIALIZATION //
////////////////////

// Logs the server's main sockets configuration
void	Server::logMainSockets(void)
{
	std::vector<t_mainSocket>::iterator			msIt;
	std::set<int>::iterator						indsIt;

	std::cout << "<<< MAIN SOCKETS\n";
	for (msIt = this->_mainSockets.begin(); msIt != this->_mainSockets.end(); msIt++)
	{
		std::cout << "Main socket fd " << msIt->fd << ", portaddr " << msIt->portaddr
			<< ", listening for virtual servers with indexes : { ";
		for (indsIt = msIt->vservIndexes.begin(); indsIt != msIt->vservIndexes.end(); indsIt++)
			std::cout << *indsIt << " ";
		std::cout << "}\n";
	}
	std::cout << ">>>\n\n";
}

// Ensures that the server has a main socket listening to <portaddr>
// Virtual server with index <vservInd> has to listen to <portaddr>, so either
// 		\ a main socket listens already to <portaddr>
// 			-> <vservInd> is simply added to the list of virtual servers for which this socket listens
// 		\ there is no main socket listening to <portaddr>
// 			-> a main socket is created with only <vservInd> (for now) in its list
void	Server::addPortaddrToWebserv(const t_portaddr& portaddr, unsigned int vservInd)
{
	t_mainSocket								ms;
	std::vector<t_mainSocket>::iterator			mainSocketIt;
	
	for (mainSocketIt = this->_mainSockets.begin(); mainSocketIt != this->_mainSockets.end(); mainSocketIt++)
	{
		if (portaddrContains((*mainSocketIt).portaddr, portaddr))
		{
			(*mainSocketIt).vservIndexes.insert(vservInd);
			return ;
		}
	}
	ms.fd = 0;
	ms.portaddr = portaddr;
	ms.vservIndexes.insert(vservInd);
	this->_mainSockets.push_back(ms);
}

// Creates the server's list of main sockets (which are only <t_mainSocket> structs,
// no socket is really opened with function <socket> until call to <startServer>)
// Goes through the list of virtual servers twice :
// 		\ first for portaddrs where the interface IP is "INADDR_ANY" (<=> any IP)
// 		\ second for portaddrs where the interface IP is a specific IP
// (this order avoids creating a <t_mainSocket> for a specific port and specific IP,
// 	then having to replace it because another virtual server listens on the same port but on any IP)
// Thus if 2 virtual servers listen on the same portaddr,
// only one <t_mainSocket> is created that listens to that portaddr for both virtual servers
// 		-> each <t_mainSocket> struct has a list of the indexes of virtual servers
// 		   for which it is listening
// 		-> identification of the specific virtual server to which a request is directed
// 		   is done after request parsing, using the virtual servers' names)
// [cfg feature] configure portaddr on which vserv is listening
void	Server::initMainSockets(void)
{
	unsigned int						vservInd;
	t_vserver*							vserv;
	std::set<t_portaddr >::iterator		portaddrIt;

	for (vservInd = 0; vservInd < this->_vservers.size(); vservInd++) // loop for portaddrs with 'any IP'
	{
		vserv = &(this->_vservers[vservInd]);
		for (portaddrIt = (*vserv).portaddrs.begin(); portaddrIt != (*vserv).portaddrs.end(); portaddrIt++)
		{
			if ((*portaddrIt).first == INADDR_ANY)
				this->addPortaddrToWebserv(*portaddrIt, vservInd);
		}
	}
	for (vservInd = 0; vservInd < this->_vservers.size(); vservInd++) // loop for portaddrs with 'specific IP'
	{
		vserv = &(this->_vservers[vservInd]);
		for (portaddrIt = (*vserv).portaddrs.begin(); portaddrIt != (*vserv).portaddrs.end(); portaddrIt++)
		{
			if ((*portaddrIt).first != INADDR_ANY)
				this->addPortaddrToWebserv(*portaddrIt, vservInd);
		}
	}
}

/////////////////////
// STARTING SERVER //
/////////////////////

// Opens all main sockets defined by the <t_mainSocket> structs in vector <_mainSockets>
int		Server::startServer(void)
{
	unsigned int		ind;

	std::cout << "[startServer] opening main sockets of server\n";
	for (ind = 0; ind < this->_mainSockets.size(); ind++)
	{
		if (this->openMainSocket(this->_mainSockets[ind]))
			return (1);
	}
	return (0);
}


// Calls three functions to setup a main socket of the server,
// using the <portaddr> field of parameter <ms> to know where to bind it :
// 		\ <socket> : create the socket (represented by a file, with a fd as susual)
// 		\ <bind> : define on which IPaddress and port the socket listens
// 				   (using a structure provided by the library and which has to be cast in the function call)
// 		\ <listen> : have the socket start listening, ie. registering incoming connections in a queue
// 					 which can hold a queue of MAX_QUEUED_CONNECTIONS requests before refusing connections
// Details :
// 		\ if the IPaddress (in <ms.portaddr.first>) is INADDR_ANY
// 			it means accepting incoming connections received on any of our device's interfaces
// 				(since a device can have several interfaces, each with a different IP)
// 		\ functions <htonl> and <htons> are used to convert bit representations of address and port numbers
// 		  to big-endian (network order) to small-endian (usual OS order)
// 		\ function <setsockopt> is used to allow webserv to bind its main socket to a port
//	 		even if there still is an active connection using that port
// 				that is in currently in TIME_WAIT state (usually from a previous Webserv closing)
int		Server::openMainSocket(t_mainSocket& ms)
{
	struct sockaddr_in	socketAddress;
	int					reuseaddrOptionValue = 1;

	std::cout << "[startServer] creating a main socket for portaddr " << ms.portaddr << "\n";
	ms.fd = socket(AF_INET, SOCK_STREAM, 0);
	if (ms.fd < 0)
		return (logError("[startServer] error when creating socket", 1));
	std::cout << "\tsocket created with fd " << ms.fd << "\n";

	setsockopt(ms.fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddrOptionValue, sizeof(reuseaddrOptionValue));
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons(ms.portaddr.second);
	socketAddress.sin_addr.s_addr = htonl(ms.portaddr.first);
	if (bind(ms.fd, reinterpret_cast<sockaddr*>(&socketAddress), sizeof(socketAddress)) < 0)
		return (logError("[startServer] error when binding socket to address", 1));
	std::cout << "\tsocket bound "
		<< "(IP " << inet_ntoa(socketAddress.sin_addr)
		<< ", port : " << ms.portaddr.second << ")\n";

	if (listen(ms.fd, MAX_QUEUED_CONNECTIONS) < 0)
		return (logError("[startServer] error when passing socket into 'listen' mode", 1));
	std::cout << "\tsocket passed into 'listen' mode\n";
	return (0);
}

// Frees all instances of <ClientHandler> (which closes all sockets currently connected to a client),
// then closes all main sockets
// Will only be called when a SIGINT stops the server
// (the server loop is infinite, so nothing can call <stopServer> except a signal handler)
void	Server::stopServer(void)
{
	unsigned int		ind;

	std::cout << "[stopServer] Closing main sockets of server\n";
	for (ind = 0; ind < this->_clientHandlers.size(); ind++)
		delete this->_clientHandlers[ind];
	for (ind = 0; ind < this->_mainSockets.size(); ind++)
		close(this->_mainSockets[ind].fd);
	g_global_signal = 0;
}

/////////////////////
// SERVER MAINLOOP //
/////////////////////

// Loops indefinitely to answer requests made to the server,
// 		may only be interrupted by signal SIGINT
// Uses function <poll> to checks which sockets are ready to perform I/O operations
// (among all sockets connected to a client + all main sockets)
// At each cycle :
// 		\ build an array of structs representing sockets that may require an I/O
// 		  	~ by asking the registered <ClientHandler> instances for their socket fds,
// 			  and which event they expect (POLLIN or POLLOUT)
// 		  	~ by adding all the main socket fds (always expecting POLLIN)
// 		\ call function <poll> to fill the array with, for each socket, the events it encountered
// 			-> POLLIN for "received data that is ready for reading",
// 			   POLLOUT for "ready to send the server response to the client")
// 			/!\ <poll> called without duration, so it will wait indefinitely
// 				until either an event does occur on a socket, or the program receives a signal
// 		\ (check if <poll> returned because the program received SIGINT, if so the program must exit)
// 		\ call <goThroughEvents> to go through the array to manage occurring events
// 			~ on dedicated sockets : transmit event to corresponding <ClientHandler> instance
// 			~ on main sockets : call <accept> to accept connection from a new client
// 		\ clean the internal vector holding the instances of <ClientHandler>
// 		  from connections that were closed during this cycle
// 			(ie. encountered an empty read on socket that had POLLIN event)
// So eg. a client requesting a unique resource appears in 4 <poll> loop cycles :
// 		1. POLLIN on main socket, client gets accepted (new socket created)
// 		2. POLLIN on dedicated socket, request gets read
// 		3. POLLOUT on dedicated socket, response gets sent (it is short enough to require only 1 send)
// 		4. POLLIN on dedicated socket, empty read since client left -> connection is closed
int		Server::serverLoop(void)
{
	struct pollfd*	pfds;
	unsigned int	nfds;
	int				nfdsReady;

	std::cout << "\n\033[35mEntering server mainloop\033[0m\n";
	while (1)
	{
		std::cout << "\n<<< LOOP CYCLE AT ";
		logTime(1);
		nfds = this->createPollFds(&pfds);
		nfdsReady = poll(pfds, nfds, -1);
		if (g_global_signal) // in case of interruption by SIGINT during poll
			return (delete[] pfds, this->stopServer(), 0);
		if (nfdsReady == -1)
		{
			logError("[serverLoop] error during poll", 1);
			return (delete[] pfds, this->stopServer(), 0);
		}
		std::cout << "Number of sockets ready for I/O : " << nfdsReady << "\n";
		if (nfdsReady > 0 && this->goThroughEvents(pfds, nfds))
			return (delete[] pfds, this->stopServer(), 0);
		if (g_global_signal) // in case of interruption by SIGINT during CGI execution
			return (delete[] pfds, this->stopServer(), 0);
		this->removeClosedConnections();
		delete[] pfds;
		std::cout << ">>>\n";
	}
	return (0);
}

// Loops through the <poll> struct array to search for events on sockets
// identified by the flags set in the <revents> member of the struct
// If some event flags are set, event is redirected to
// 		\ <handleOldConnection> if socket is a dedicated socket,
// 			which will in turn call the handling method of corresponding <clientHandler> instance
// 		\ <handleNewConnection> if socket is main socket
// 			<=> the connection is new and requires a call to <accept> on this main socket
// <handleOldConnection>'s return value is checked but should never be 1,
// since error on clients are ignored (the connection with the client is simply stopped)
// in order to maintain server running
int		Server::goThroughEvents(struct pollfd* pfds, unsigned int nfds)
{
	unsigned int	socketInd;

	for (socketInd = 0; socketInd < nfds; socketInd++)
	{
		if (pfds[socketInd].revents != 0)
		{
			std::cout << "\tSocket fd " << pfds[socketInd].fd << " is ready, events : ";
			this->displayEvents(pfds[socketInd].revents);
			if (socketInd < this->_mainSockets.size() && handleNewConnection(socketInd))
				return (1);
			if (socketInd >= this->_mainSockets.size()
				&& this->handleOldConnection(socketInd, pfds[socketInd]))
				return (1);
		}
	}
	return (0);
}

// Redirects event on dedicated socket to the corresponding <clientHandler> instance,
// and uses its return value to check if
// 		\ return value 1 : a fatal error occurred on the connection
// 		\ return value 2 : the connection must be ended because of client hangup or malformed request
// -> in both cases, <ClientHandler> instance must be freed
//    and the pointer to the instance removed from <_clientHandlers>
// /!\ Fatal errors in the Client are not fatal to the Server,
// 	   to respect the principle "server should remain available at all times"
int		Server::handleOldConnection(unsigned int socketInd, struct pollfd& pfd)
{
	int				ret;
	unsigned int	clientInd = socketInd - this->_mainSockets.size();

	ret = this->_clientHandlers[clientInd]->handleEvent(pfd);
	if (ret == 1 || ret == 2)
	{
		delete this->_clientHandlers[clientInd];
		this->_clientHandlers[clientInd] = NULL;
	}
	return (0);
}

// Handles new connections received by the (currently listening) main socket,
// 		ie. connections from clients that are not already connected to the server
// This function is only called if <poll> detected movement on a main socket,
// 		so it is certain that there are connections waiting in this main socket's queue :
// 			the call to <accept> will select the first one, remove it from the queue,
// 			and create a new socket (+ an instance of Clienthandler dedicated to that client)
// /!\ The IPaddress+port stored in <clientAddress> by function <accept> is the CLIENT address,
// 	   ie. the address of the client's device where the client receives packets :
// 			when we call function `send` to send data to client, this address will be used as destination of packet
// 		(but we do not need to keep it in memory ourselves, ii is managed by the OS)
int		Server::handleNewConnection(unsigned int mainSocketInd)
{
	struct sockaddr_in	clientAddress;
	socklen_t			clientAddressSize;
	t_portaddr			clientPortaddr;
	int					clientSocketFd;

	clientAddressSize = sizeof(clientAddress);
	clientSocketFd = accept(this->_mainSockets[mainSocketInd].fd,
		reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressSize);
	if (clientSocketFd < 0)
		return (logError("[handleNewConnection] error when trying to accept an incoming connection", 1));
	clientPortaddr.first = ntohl(clientAddress.sin_addr.s_addr);
	clientPortaddr.second = ntohs(clientAddress.sin_port);
	std::cout << "\t[handleNewConnection] connection accepted in socket fd "
		<< clientSocketFd << " (client portaddr : " << clientPortaddr << ")\n";
	this->_clientHandlers.push_back(
		new Client(clientSocketFd, clientPortaddr, this->_mainSockets[mainSocketInd], this->_vservers));
	return (0);
}

///////////
// UTILS //
///////////

// Removes from the <_clientHandlers> vector the free spaces left by closed connections
// (executed after each <poll> loop cycle)
void	Server::removeClosedConnections(void)
{
	int	ind;

	//std::cout << "Removing handlers of closed connections from client handlers array\n";
	for (ind = this->_clientHandlers.size() - 1; ind >= 0; ind--)
	{
		if (this->_clientHandlers[ind] == NULL)
		{
			std::cout << "Removing client handler at index " << ind << "\n";
			this->_clientHandlers.erase(this->_clientHandlers.begin() + ind);
		}
	}
}

// Creates the <poll> struct array :
// 		\ one pollfd for each main socket (expected event is always POLLIN)
// 		\ one struct for each connection to a client
// 			(expected events determined by the corresponding <ClientHandler> instance)
unsigned int		Server::createPollFds(struct pollfd **pfds)
{
	unsigned int	nfds = this->_mainSockets.size() + this->_clientHandlers.size();
	unsigned int	ind;
	unsigned int	clientInd;

	std::cout << "Currently maintaining " << nfds << " sockets, incl "
		<< this->_clientHandlers.size() << " client sockets\n";
	*pfds = new struct pollfd [nfds];
	for (ind = 0; ind < this->_mainSockets.size(); ind++)
	{
		(*pfds)[ind].fd = this->_mainSockets[ind].fd;
		(*pfds)[ind].events = POLLIN;
		std::cout << "\tmain socket fd " << (*pfds)[ind].fd
			<< " at " << this->_mainSockets[ind].portaddr << " awaits events : ";
		this->displayEvents((*pfds)[ind].events);
	}
	for (clientInd = 0; clientInd < this->_clientHandlers.size(); clientInd++)
	{
		ind = this->_mainSockets.size() + clientInd;
		this->_clientHandlers[clientInd]->preparePollFd((*pfds)[ind]);
		std::cout << "\tclient socket fd " << (*pfds)[ind].fd
			<< " at " << this->_clientHandlers[clientInd]->getMainSocket().portaddr << " awaits events : ";
		this->displayEvents((*pfds)[ind].events);
	}
	return (nfds);
}

// Log function for event flags in the <poll> structs
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
