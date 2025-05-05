#ifndef CLIENT_H
# define CLIENT_H

# include "webserv.hpp"

enum ClientState
{
	RECEIVING,
	SENDING,
	CLOSED
};

class	Client;

class	Client
{
	public:
		Client(int socketFd,
			t_mainSocket& mainSocket, std::vector<t_vserver>& vservers);
		~Client( void );
		// Mainloop
		void					preparePollFd( struct pollfd& pfd );
		int						handleEvent( struct pollfd& pfd );
		// Accessors
		int						getSocketFd( void ) const;
		t_mainSocket&			getMainSocket(void);
	private:
		int						receiveHTTP( void );
		int						handleCompleteRequest( void );
		int						sendHTTP( void );

		int								_socketFd;
		t_mainSocket&					_mainSocket;
		std::vector<t_vserver>&			_vservers;
		int								_state;
		unsigned int					_nbytesSent_tmp;

		std::string						_request_buffer;
		std::string						_response_buffer;
		bool							_response_sent;
};

#endif
