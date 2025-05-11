#include "../hpp_files/webserv.hpp"

// Trims a string, ie. removes all space characters on the left and right
void	trimString(std::string& s)
{
	unsigned int	ind;

	ind = 0;
	while (ind < s.size() && std::isspace(s[ind]))
		ind++;
	if (ind > 0)
		s.erase(0, ind);
	if (s.size() == 0)
		return ;
	ind = s.size() - 1;
	while (ind >= 0 && std::isspace(s[ind]))
		ind--;
	if (ind < s.size() - 1)
		s.erase(ind + 1, s.size() - ind);
}

// Splits strings <orig> by separators in <seps>
// and puts the resulting tokens in vector <tokens>
int	splitString(std::string& orig, std::vector<std::string>& tokens, std::string seps)
{
	unsigned int	ind = 0;
	unsigned int	indLast = 0;

	while (ind < orig.size())
	{
		while (ind < orig.size() && seps.find(orig[ind]) == std::string::npos)
			ind++;
		if (ind != indLast)
			tokens.push_back(orig.substr(indLast, ind - indLast));
		ind++;
		indLast = ind;
	}
	return (0);
}

// Checks if <str> ends with <suffix>
bool endsWith(const std::string& str, const std::string& suffix)
{
	if (suffix.size() > str.size())
		return (false);
	return (std::equal(suffix.rbegin(), suffix.rend(), str.rbegin()));
}

// Creates a new string resulting from the concatenation of 2 strings, with 2 caveats
// 		\ allocates with <new>, so no checks on allocation success
// 		\ <s1> length determined by first '\0', but <s2> length is given as argument
// 			(reason : function will be called with <s2> as the result of last read on socket,
// 			 so it may be part of request body and contain non-terminating '\0' if body is binary)
char*	ft_strjoinDefsize(char* s1, char* s2, size_t sizes2)
{
	char*		res;
	size_t		ind;
	size_t		sizes1;

	sizes1 = 0;
	while (s1[sizes1])
		sizes1++;
	res = new char[sizes1 + sizes2 + 1];
	res[sizes1 + sizes2] = '\0';
	for (ind = 0; ind < sizes1; ind++)
		res[ind] = s1[ind];
	for (; ind < sizes1 + sizes2; ind++)
		res[ind] = s2[ind - sizes1];
	return (res);
}