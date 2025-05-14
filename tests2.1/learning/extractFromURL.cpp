#include <string>
#include <vector>
#include <iostream>

int	main(int ac, char **av)
{
	if (ac != 2)
		return (1);

	std::string		path(av[1]);
	std::string		extension = "";
	std::string		filePath = "";
	std::string		queryString = "";
	std::string		pathInfo = "";
	bool			toDir = 0;
	unsigned long	ind;
	unsigned long	ind2;

	ind = path.find('?');
	if (ind != std::string::npos && ind + 1 < path.size())
		queryString = path.substr(ind + 1);
	filePath = path.substr(0, ind);

	ind = filePath.find('.');
	if (ind != std::string::npos)
	{
		ind2 = filePath.find('/', ind);
		if (ind2 != std::string::npos)
			pathInfo = filePath.substr(ind2);
		extension = filePath.substr(ind, ind2 - ind);
		filePath = filePath.substr(0, ind2);
	}
	else
		toDir = (filePath.back() == '/');
	
	std::cout << "Extracted from path " << path << " :\n"
		<< "\tfilePath\t\t " << filePath << "\n"
		<< "\ttoDir\t\t\t " << toDir << "\n"
		<< "\textension\t\t " << extension << "\n"
		<< "\tpathInfo\t\t " << pathInfo << "\n"
		<< "\tqueryString\t\t " << queryString << "\n";
	return (0);
}