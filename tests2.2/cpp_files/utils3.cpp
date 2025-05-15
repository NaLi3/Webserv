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
