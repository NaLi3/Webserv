#ifndef CFGPARSING_HPP
# define CFGPARSING_HPP

# include "webserv.hpp"

// Stores parameters for a specific location of a virtual server
// 		\ locationPath : path that URL requested by clients must match to get in this location
// 		\ redirection : path that must replace <locationPath> before starting process again with new URL
// 			(if set, no other of the next parameters can be set)
// 		\ rootPath : path to prepend to URL to get to the right directory in server's arborescence
// 		\ acceptedMethods : names ('GET', 'POST', ...) of allowed request methods for this location
// 		\ cgiExtensions : list of file extensions associated that require to execute a CGI
// 		\ uploadPath : path (appended to <rootPath>) in which uploaded files must be stored
// 		\ defaultFileWhenDir : path to index file to serve if a GET requests a directory URL
// 		\ autoIndex : boolean defining wether the server can generate index files itself
// 			(if set, <defaultFileWhenDir> can't be set)
struct s_location
{
	std::string							locationPath;
	std::string							redirection;
	std::string							rootPath;
	std::set<std::string>				acceptedMethods;
	bool								acceptedMethodsSet;
	std::map<std::string, std::string>	cgiExtensions;
	std::string							uploadPath;
	std::string							defaultFileWhenDir;
	bool								autoIndex;
	bool								autoIndexSet;
};

// Stores parameters for a virtual server
// 		\ portaddrs : set of pairs <listenAddress, listenPort> to which the server listens
// 			/!\ IP addresses and port numbers stored in host byte order, ie. little-endian :
// 				to use them with function <bind>, convert to network byte order thanks to <htonl>/<htons>
// 		\ serverNames : list of names of the server, used to diambiguate request destination
// 		  if a request is received on a portaddr on which multiple virtual servers are listening
// 		\ errorPages : map associating an error page to an error code
// 		\ maxRequestBodySize : max size of a client request, in ?? (#f : determine unit)
// 		\ locations : list of locations parameters structures
struct s_vserver
{
	std::set<std::pair<uint32_t, uint16_t> >	portaddrs; // set of pairs <listenAddress, listenPort>
	bool										listenSet;
	std::set<std::string>						serverNames;
	std::map<int, std::string>					errorPages;
	long										maxRequestBodySize;
	std::vector<t_location>						locations;
};

std::ostream&	operator<<(std::ostream& out, const std::pair<uint32_t, uint16_t>& portaddr);
void			logWebservConf(std::vector<t_vserver> vservers);
int				portaddrContains(const std::pair<uint32_t, uint16_t>& portaddrA, const std::pair<uint32_t, uint16_t>& portaddrB);
int				parseCfgFile(std::string& cfgFileName, std::vector<t_vserver>& vservers);

#endif