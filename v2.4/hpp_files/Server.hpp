#ifndef SERVER_H
# define SERVER_H

# include "webserv.hpp"
# define MAX_QUEUED_CONNECTIONS 10

// Reads a config file to create a list of main sockets that will listen on portaddrs,
// and handles through instances of <Client> the dedicated sockets that will be created
// for clients that connected to the main sockets
// 	-> in <serverLoop>, all sockets are searched repeatedly for IO events using function <poll>
class	Server
{
	public:
		Server();
		~Server();
		int			initServer(std::string cfgFileName);
		int			startServer();
		int			serverLoop();
		void		stopServer();

	private:
		std::vector<t_vserver>		_vservers;
		std::vector<t_mainSocket>	_mainSockets;
		std::vector<Client*>		_clientHandlers;
		// Initialization
		void						initMainSockets();
		void						addPortaddrToWebserv(const t_portaddr& portaddr, unsigned int vservInd);
		void						logMainSockets();
		// Mainloop
		int							openMainSocket(t_mainSocket& ms);
		int							goThroughEvents(struct pollfd* pfds, unsigned int nfds);
		int							handleNewConnection(unsigned int mainSocketInd);
		int							handleOldConnection(unsigned int clientInd, struct pollfd& pfd);
		// Utils
		void						removeClosedConnections();
		unsigned int				createPollFds(struct pollfd **pfds);
		void						displayEvents(short revents);
};

// Stores information about a main socket of Webserv :
// 		\ fd : its socket file descriptor
// 		\ portaddr : where it listens for incoming connections
// 		\ vservIndexes : (the indexes in Server::_vservers of) the virtual servers
// 						 which listen on this portaddr, and so may have to handle requests received here
struct s_mainSocket
{
	int					fd;
	t_portaddr			portaddr;
	std::set<int>		vservIndexes;
};

#endif
