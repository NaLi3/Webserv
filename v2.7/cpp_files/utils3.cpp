#include "../hpp_files/webserv.hpp"

// Searches the bytes starting at <mem> for a sequence of bytes identical to <needle>
// Does not stop on null byte '\0', so the memory and needle sizes MUST be provided
ssize_t	memFind(const char *mem, size_t memSize, const char *needle, size_t needleSize)
{
	ssize_t	memInd;
	ssize_t	needleInd;
	ssize_t	smemSize = static_cast<ssize_t>(memSize);
	ssize_t sneedleSize = static_cast<ssize_t>(needleSize);

	memInd = 0;
	while (memInd < smemSize)
	{
		needleInd = 0;
		while (needleInd < sneedleSize && memInd < smemSize
			&& mem[memInd + needleInd] == needle[needleInd])
			needleInd++;
		if (needleInd == sneedleSize)
			return (memInd);
		memInd++;
	}
	return (-1);
}

// Divides <path> into its directory part <dirPath> and its filename part <filename>
// (the directory part has a trailing '/')
// Returns 1 if no '/' separator was found
int	divideFilePath(std::string& path, std::string& dirPath, std::string& filename)
{
	size_t ind;

	ind = path.find_last_of('/');
	if (ind == std::string::npos)
		return (1);
	dirPath = path.substr(0, ind + 1);
	filename = path.substr(ind + 1);
	return (0);
}

// Erases the last character in a string
// (function used for readability, because <s.back()> does not exist in C++98)
void	eraseLastChar(std::string& s)
{
	if (s.size() > 0)
		s.erase(s.size() - 1);
}

// Duplicate a C-string like <strdup>, except it uses <new> instead of <malloc>
char	*duplicateCstr(const char *src)
{
	char	*res;
	size_t	siz;

	siz = strlen(src);
	res = new char[siz + 1];
	bzero(res, siz + 1);
	strcpy(res, src);
	return (res);
}

// Capitalizes a string and replaces its '-' with '_',
// to make it suitable as an env variable name
void	transformToEnvName(std::string& s)
{
	size_t	ind;

	for (ind = 0; ind < s.size(); ind++)
	{
		if (s[ind] >= 'a' && s[ind] <= 'z')
			s[ind] += 'A' - 'a';
		if (s[ind] == '-')
			s[ind] = '_';
	}
}
