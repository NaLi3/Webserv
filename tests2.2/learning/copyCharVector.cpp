#include <string>
#include <vector>
#include <iostream>

void	logVector(std::vector<char> o)
{
	unsigned int		ind;

	std::cout << "\033[32m<\n";
	for(ind = 0; ind < o.size(); ind++)
		std::cout << o[ind];
	std::cout << "\n>\n\033[0m";
}

int	main(void)
{
	std::vector<char>	v1;
	std::vector<char>	v2;

	v1.push_back('a');
	v1.push_back('b');
	logVector(v1);
	v2 = std::vector<char>(v1.begin(), v1.end());
	logVector(v2);
	return (0);
}