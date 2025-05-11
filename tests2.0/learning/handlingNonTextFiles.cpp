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

int	main(void)
{
	std::ifstream  in ("input_file");
	std::ofstream  out ("output_file");

	out << in;

	in.close();
	out.close();
}
