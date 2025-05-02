#ifndef SERVER_H
# define SERVER_H

# include "webserv.hpp"

# define MAX_QUEUED_CONNECTIONS 10

typedef struct s_mainSocket t_mainSocket;

class	Server
{
	public:
		Server(std::string cfgFileName);
		~Server();
		int			startServer(void);
		int			serverLoop(void);
		void		stopServer(void);

	private:
		std::vector<t_vserver>		_vservers;
		std::vector<t_mainSocket>	_mainSockets;
		std::vector<Client*>		_clientHandlers;
		// Initialization
		void		initMainSockets(void);
		void		addPortaddrToWebserv(const std::pair<uint32_t, uint16_t>& portaddr, unsigned int vservInd);
		void		logMainSockets(void);
		// Mainloop
		int			openMainSocket(t_mainSocket& ms);
		int			goThroughEvents(struct pollfd* pfds, unsigned int nfds);
		int			handleNewConnection(unsigned int mainSocketInd);
		int			handleOldConnection(unsigned int clientInd, struct pollfd& pfd);
		// Utils
		void			removeClosedConnections(void);
		unsigned int	createPollFds(struct pollfd **pfds);
		void			displayEvents(short revents);
};

// Stores information about a main socket of Webserv :
// 		\ fd : its socket file descriptor
// 		\ portaddr : where it listens for incoming connections
// 		\ vservIndexes : (the indexes in Server::_vservers of) the virtual servers
// 						 which listen on this portaddr, and so may have to handle requests received here
struct s_mainSocket
{
	int								fd;
	std::pair<uint32_t, uint16_t>	portaddr;
	std::set<int>					vservIndexes;
};

#endif
