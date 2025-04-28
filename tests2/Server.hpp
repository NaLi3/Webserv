#ifndef SERVER_H
# define SERVER_H

# include <string>
# include <fstream>
# include <iostream>
# include <stdlib.h>
# include <strings.h>
# include <fcntl.h>
# include <unistd.h>
# include <errno.h>
# include <vector>

# include <sys/socket.h>
# include <arpa/inet.h>
# include <poll.h>

# include "ClientHandler.hpp"

# define MAX_QUEUED_CONNECTIONS 10

class	Server;
class	ClientHandler;

class	Server
{
	public:
		Server(std::string cfgFileName);
		~Server();
		int			startServer(void);
		int			serverLoop(void);


	private:
		uint16_t					_port;
		int							_mainSocketFd;
		std::vector<ClientHandler*>	clientHandlers;
		// Mainloop
		int			goThroughEvents(struct pollfd* pfds, int nfds);
		int			handleNewConnection(void);
		int			handleOldConnection(int clientInd, struct pollfd& pfd);
		// Utils
		void		removeClosedConnections(void);
		int			createPollFds(struct pollfd **pfds);
		void		displayEvents(short revents);
};

int	logError(std::string msg, bool displayErrno);

#endif
