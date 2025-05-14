// Generic modules
# include <cctype>
# include <string>
# include <iostream>
# include <stdlib.h>
# include <strings.h>
# include <unistd.h>
# include <limits>
# include <errno.h>
# include <vector>
# include <map>
# include <stack>
# include <set>
# include <fstream>
# include <fcntl.h>
# include <sys/stat.h>
# include <csignal>

// Sockets modules
# include <sys/socket.h>
# include <arpa/inet.h>
# include <poll.h>

int	logError(std::string msg, bool displayErrno)
{
	std::cout << msg << "\n";
	if (displayErrno)
		perror("Error description");
	return (1);
}

int		startSocket(short _port, std::string _ip)
{
	struct sockaddr_in	socketAddress;
	int					reuseaddrOptionValue = 1;
	int					_mainSocketFd;

	_mainSocketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_mainSocketFd < 0)
		return (logError("[startServer] error when creating main socket", 1));
	std::cout << "[startServer] main socket created with fd " << _mainSocketFd << "\n";

	setsockopt(_mainSocketFd, SOL_SOCKET, SO_REUSEADDR, &reuseaddrOptionValue, sizeof(reuseaddrOptionValue));
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons(_port);
	inet_aton(_ip.c_str(), &(socketAddress.sin_addr));
	std::cout << "[startServer] address literal " << _ip << " converted to " << socketAddress.sin_addr.s_addr << "\n";
	if (bind(_mainSocketFd, reinterpret_cast<sockaddr*>(&socketAddress), sizeof(socketAddress)) < 0)
	{
		close(_mainSocketFd);
		return (logError("[startServer] error when binding socket to address", 1));
	}
	std::cout << "[startServer] main socket bound "
		<< "(IP " << inet_ntoa(socketAddress.sin_addr)
		<< ", port : " << _port << ")\n";

	if (listen(_mainSocketFd, 10) < 0)
	{
		close(_mainSocketFd);
		return (logError("[startServer] error when passing main socket into 'listen' mode", 1));
	}
	std::cout << "[startServer] main socket passed into 'listen' mode\n";
	//close(_mainSocketFd);
	return (0);
}

int	main(void)
{
	int		socket1;
	int		socket2;

	/*
	// Sockets on anyIP + same port
	// FAILS for socket2, address already in use
	socket1 = startSocket(8080, "0.0.0.0");
	if (socket1 == 1)
		return (1);
	socket2 = startSocket(8080, "0.0.0.0");
	if (socket2 == 1)
		return (close(socket1), 1);
	close(socket1);
	close(socket2);
	*/

	/*
	// Sockets on same IP + same port
	// FAILS for socket2, address already in use
	socket1 = startSocket(8080, "127.0.0.1");
	if (socket1 == 1)
		return (1);
	socket2 = startSocket(8080, "127.0.0.1");
	if (socket2 == 1)
		return (close(socket1), 1);
	close(socket1);
	close(socket2);
	*/

	// Sockets on different IP + same port (/!\ replace network IP using command `ip`, see interface enp0s)
	// SUCCEEDS
	socket1 = startSocket(8080, "192.168.122.1");
	if (socket1 == 1)
		return (1);
	socket2 = startSocket(8080, "127.0.0.1");
	if (socket2 == 1)
		return (close(socket1), 1);
	close(socket1);
	close(socket2);

	/*
	// Socket1 on anyIP + a port, socket2 on an IP + same port
	// FAILS for socket2, address already in use
	socket1 = startSocket(8080, "0.0.0.0");
	if (socket1 == 1)
		return (1);
	socket2 = startSocket(8080, "127.0.0.1");
	if (socket2 == 1)
		return (close(socket1), 1);
	close(socket1);
	close(socket2);
	*/	
}