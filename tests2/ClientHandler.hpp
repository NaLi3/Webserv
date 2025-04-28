#ifndef CLIENTHANDLER_H
# define CLIENTHANDLER_H

# include "Server.hpp"

# define RECEIVING 1
# define SENDING 2

class	ClientHandler;

class	ClientHandler
{
	public:
		ClientHandler(int clientSocketFd);
		~ClientHandler();
		// Mainloop
		void		preparePollFd(struct pollfd& pfd);
		int			handleEvent(struct pollfd& pfd);
		// Accessors
		int			getSocketFd(void);

	private:
		int			receiveHTTP(void);
		int			sendHTTP(void);
		int			_socketFd;
		int			_state;
};



#endif
