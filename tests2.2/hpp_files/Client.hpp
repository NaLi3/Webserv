#ifndef CLIENT_H
# define CLIENT_H

# include "webserv.hpp"
# define READSOCK_BUFFER_SIZE 1024UL // CANNOT BE less than 4
// 										(otherwise server won't be able to read '\r\n\r\n' in requests)

enum ClientState
{
	RECEIVING,
	SENDING,
	CLOSED
};

// Holds the logic handling the interaction with the client on a dedicated socket
// 		\ the "interaction" with the client is in fact a TCP connection managed by the OS ;
// 			by default <Client> tries to keep the connection "persistent",
// 				ie. does not close the connection after having sent a response
// 			-> go to <handleEvent> and its error values to see what could stop the connection
// 		\ <preparePollFd>, called by <Server>, defines in the socket's <pollfd> structure
// 		  which events are awaited on that socket (POLLIN/POLLOUT)
// 		\ <handleEvent>, called by <Server>, manages events received on the socket
// 			~ POLLIN events : read from the socket into the <_requestBuffer>,
// 				if the <requestBuffer> holds a complete request
// 				generate its response in <_responseBuffer>
// 			~ POLLOUT events : write as much bytes as possible from <_responseBuffer> into the socket
// /!\ Constructor takes a lot of parameters because it registers many informations
// 	   about the socket on which it listens and the server's list of virtual servers
// 			to be able to determine to which virtual server requests received on the socket are directed
class	Client
{
	public:
		Client(int socketFd, t_portaddr clientPortaddr, t_mainSocket& mainSocket, std::vector<t_vserver>& vservers);
		~Client();
		// Mainloop
		void					preparePollFd(struct pollfd& pfd);
		int						handleEvent(struct pollfd& pfd);
		// Accessors
		int						getSocketFd() const;
		t_mainSocket&			getMainSocket();

	private:
		// Actions when in SENDING mode
		void					passToReceiving();
		int						sendHTTP();
		// Actions when in RECEIVING mode
		void					passToSending();
		void					handleProblematicRequest(std::string status);
		int						handleCompleteRequest(int recurrLvl);
		int						finishRequestHeaders(char *buffer, size_t bufferSize, ssize_t indHeadersEnd);
		int						receiveHeadersPart(char *buffer, size_t bufferSize);
		int						receiveBodyPart(char* buffer, size_t bufferSize);
		int						receiveHTTP();
		// Memory of server's config
		int								_socketFd;
		t_mainSocket&					_mainSocket;
		t_portaddr						_clientPortAddr;
		std::vector<t_vserver>&			_vservers;
		// Internal state
		int								_state;
		int								_closeAfterSent;
		unsigned long					_nbytesSent;
		Response*						_response;
		char*							_responseBuffer;
		unsigned long					_responseSize;
		std::string						_requestHeaders;
		Request*						_request;
		char							_lastBuffer[READSOCK_BUFFER_SIZE + 1];
};

#endif
