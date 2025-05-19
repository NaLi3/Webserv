# include <string>
# include <iostream>
# include <unistd.h>
# include <strings.h>

int	main(void)
{
	char	buffer[8];
	long	nBytesRead = 1;

	std::cout << "Echoing form content...\n";
	while (nBytesRead > 0)
	{
		bzero(buffer, 8);
		nBytesRead = read(0, buffer, 8);
		if (nBytesRead > 0)
			std::cout << "Read : --" << buffer << "--\n";
	}
	return (0);
}
