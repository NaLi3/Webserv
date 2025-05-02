#ifndef CLIENT_H
# define CLIENT_H

# include "Webserv.hpp"

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
		Client( int clientSocketFd );
		~Client( void );
		// Mainloop
		void					preparePollFd( struct pollfd& pfd );
		int						handleEvent( struct pollfd& pfd );
		// Accessors
		int						getSocketFd( void ) const;
		const	std::string&	getRequestBuffer( void ) const;
		void					setResponse( const std::string& response_msg );
		bool					isRequestrComplete( void ) const;
	private:
		int						receiveHTTP( void );
		int						sendHTTP( void );

		int						_socketFd;
		int						_state;
		std::string				_request_buffer;
		std::string				_response_buffer;
		bool					_response_sent;
		Request					_request;
};

#endif
