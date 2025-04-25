#ifndef SERVER_H
# define SERVER_H

# include <string>
# include <fstream>
# include <iostream>
# include <stdlib.h>
# include <fcntl.h>
# include <unistd.h>
# include <errno.h>

# include <sys/socket.h>
# include <arpa/inet.h>

# define MAX_QUEUED_CONNECTIONS 10

class	Server;

class	Server
{
	public:
		Server(std::string cfgFileName);
		~Server();
		int			startServer(void);
		int			waitForConnection(void);
		int			handleClientRequest(int clientSocketFd);


	private:
		uint16_t	_port;
		int			_mainSocketFd;
};

int	logError(std::string msg, bool displayErrno);

#endif
