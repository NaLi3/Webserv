#include "../hpp_files/webserv.hpp"

///////////////////////
// CHECKS ON STRINGS //
///////////////////////

// Checks that a string contains a valid unsigned integer literal
// (<=> characters are only digits 1-9)
int	invalidUintLiteral(std::string& lit)
{
	unsigned int	ind;

	for (ind = 0; ind < lit.size(); ind++)
	{
		if (!(lit[ind] >= '0' && lit[ind] <= '9'))
			return (1);
	}
	return (0);
}

// Checks that a string contains a valid port number
int	invalidPortNumber(std::string& portLit)
{
	if (invalidUintLiteral(portLit))
		return (1);
	return (0);
}

// Checks that a string contains a valid HTTP return code
// (HTTP return codes are from 100 to 599)
int	invalidReturnCode(std::string& codeLit)
{
	long			code;

	if (invalidUintLiteral(codeLit))
		return (1);
	code = strtol(codeLit.c_str(), NULL, 10);
	if (code >= 100 && code <= 599)
		return (0);
	return (1);
}

// Checks that the next line in the file is opening a block
// <=> is exactly '{' (once trimmed) 
bool	blockOpened(std::ifstream& inStream, std::string& blockName)
{
	std::string	line;

	(void)blockName;
	std::getline(inStream, line);
	trimString(line);
	//std::cout << blockName << " Line : --" << line << "--\n";
	if (line.compare("{") == 0)
		return (1);
	else
		return (0);
}

/////////////////////////
// CHECKS ON PORTADDRS //
/////////////////////////

// Checks if <portaddrB> is redundant with <portAddrA> (<portaddrA> "contains" <portaddrB>)
// 		<=> if listening on <portaddrB> is useless when already listening on <portaddrA>
// /!\ not symmetrical : checks if A contains B, not if B contains A
int	portaddrContains(const t_portaddr& portaddrA,
	const t_portaddr& portaddrB)
{
	if (portaddrA.second == portaddrB.second) // same port and ...
	{
		if (portaddrA.first == portaddrB.first) // ... either same interface IP
			return (1);
		if (portaddrA.first == INADDR_ANY) // ... or portaddrA is on any interface
			return (1);
	}
	return (0);
}

// Inserts <portaddr> into the list of portaddr to which <vs> is listening, checking if
// 		\ a portaddr that contains the new one / is identical to the new one
// 		  is already in the list -> no need to insert
// 		\ a portaddr that is contained by the new one is already in the list
// 		  -> the new one should replace the old one
void	addPortaddrToVserv(t_vserver& vs, t_portaddr& portaddr)
{
	std::set<t_portaddr >::iterator	it;

	for (it = vs.portaddrs.begin(); it != vs.portaddrs.end(); it++)
	{
		if (portaddrContains(*it, portaddr))
		{
			//std::cout << "[serv] portaddr " << portaddr
			//	<< " not added to vserv because contained by " << *it << "\n";
			return ;
		}
		else if (portaddrContains(portaddr, *it))
		{
			//std::cout << "[serv] portaddr " << portaddr
			//	<< "contains and replaces in vserv former portaddr " << *it << "\n";
			vs.portaddrs.erase(it);
			vs.portaddrs.insert(portaddr);
			return ;
		}
	}
	vs.portaddrs.insert(portaddr);
}

///////////////////////////////////////
// STRUCTURES INITIALIZATION AND LOG //
///////////////////////////////////////

// Display overload for portaddr type
std::ostream&	operator<<(std::ostream& out, const t_portaddr& portaddr)
{
	in_addr	address;

	address.s_addr = htonl(portaddr.first);
	out << "[" << inet_ntoa(address) << ":" << portaddr.second << "]";
	return (out);
}

// Display overload for virtual server (only displays 1 server name)
std::ostream&	operator<<(std::ostream& out, const t_vserver* vserv)
{
	if (vserv)
		out << "[" << *(vserv->serverNames.begin()) << "]";
	else
		out << "NULL";
	return (out);
}

// Display overload for location (only displays locationPath)
std::ostream&	operator<<(std::ostream& out, const t_location* location)
{
	if (location)
		out << "[" << location->locationPath << "]";
	else
		out << "NULL";
	return (out);
}

// Logs the parameters for 1 location in a virtual server
void	logLocation(t_location& l)
{
	std::set<std::string>::iterator		it1;
	std::vector<std::string>::iterator	it2;

	std::cout << "\tLOCATION\n";
	std::cout << "\t\tlocationPath : " << l.locationPath
		<< "\n\t\tredirection : " << l.redirection
		<< "\n\t\trootPath : " << l.rootPath
		<< "\n\t\taliasPath : " << l.aliasPath
		<< "\n\t\tuploadPath : " << l.uploadPath
		<< "\n\t\tdefaultFileWhenDir : " << l.defaultFileWhenDir
		<< "\n\t\tautoindex : " << l.autoIndex << "\n";
	std::cout << "\t\taccepted methods : ";
	for (it1 = l.acceptedMethods.begin(); it1 != l.acceptedMethods.end(); it1++)
		std::cout << *it1 << " ";
	std::cout << "\n\t\tcgis : { ";
	for (it2 = l.cgiExtensions.begin(); it2 != l.cgiExtensions.end(); it2++)
		std::cout << *it2 << " ";
	std::cout << "}\n";
}

// Logs the parameters for 1 virtual server
void	logVserver(t_vserver& vs, unsigned int vservInd)
{
	unsigned int										ind;
	std::set<t_portaddr >::iterator	it0;
	std::set<std::string>::iterator						it1;
	std::map<int, std::string>::iterator				it2;

	std::cout << "VIRTUAL SERVER INDEX " << vservInd << "\n";
	std::cout << "portaddrs :\n";
	for (it0 = vs.portaddrs.begin(); it0 != vs.portaddrs.end(); it0++)
		std::cout << "\t" << *it0 << "\n";
	std::cout << "maxRequestBodySize : " << vs.maxRequestBodySize << "\n";
	std::cout << "server names : ";
	for (it1 = vs.serverNames.begin(); it1 != vs.serverNames.end(); it1++)
		std::cout << *it1 << " ";
	std::cout << "\nerror pages :\n";
	for (it2 = vs.errorPages.begin(); it2 != vs.errorPages.end(); it2++)
		std::cout << "\t" << (*it2).first << " : " << (*it2).second << "\n";
	std::cout << "locations :\n";
	for (ind = 0; ind < vs.locations.size(); ind++)
		logLocation(vs.locations[ind]);
	std::cout << "rootPath : " << vs.rootPath << "\n";
}

// Logs all the virtual servers resulting from the configuration
void	logWebservConf(std::vector<t_vserver> vservers)
{
	unsigned int	ind;

	std::cout << "<<< VIRTUAL SERVERS OF CURRENT WEBSERV CFG\n";
	for (ind = 0; ind < vservers.size(); ind++)
		logVserver(vservers[ind], ind);
	std::cout << ">>>\n";
}

// Sets default values for 1 location in a virtual server :
// 		all information empty, except that 'GET' method is accepted by default
void	initLocation(t_location& l)
{
	l.locationPath = "";
	l.redirection = "";
	l.rootPath = "";
	l.aliasPath = "";
	l.uploadPath = "";
	l.defaultFileWhenDir = "";
	l.autoIndex = 0;
	l.autoIndexSet = 0;
	l.acceptedMethods.insert("GET"); // #f : check default value
	l.acceptedMethodsSet = 0;
}

// Sets default values for 1 virtual server :
// 		listen to port 80 on any interface of the device, no limits on request size
void	initVserver(t_vserver& vs)
{
	vs.listenSet = 0;
	vs.maxRequestBodySize = std::numeric_limits<long>::max();
	vs.rootPath = "";
}

/////////////////////
// PARSING BY LINE //
/////////////////////

// Checks if there are incompatibilities in a location block
// 		\ if there is a redirection, there can't be anything else
// 		\ if there is a default index file, autoindex can't be 1
// 		\ if there is a rootPath, there can't be an aliasPath
int	invalidLocationParams(t_location& l)
{
	if (l.redirection.size() > 0
		&& (l.rootPath.size() > 0 || l.aliasPath.size() > 0 || l.uploadPath.size() > 0
			|| l.defaultFileWhenDir.size() > 0 || l.autoIndexSet
			|| l.cgiExtensions.size() > 0 || l.acceptedMethodsSet))
		return (logError("[loca] redirection with other parameters is invalid\n", 0));
	if (l.autoIndex && l.defaultFileWhenDir.size() > 0)
		return (logError("[loca] autoindex ON while default file set is invalid\n", 0));
	if (l.aliasPath.size() > 0 && l.rootPath.size() > 0)
		return (logError("[loca] root path and alias path both defined\n", 0));
	return (0);
}

// Extracts data from line in 'location' block,
// which may only be directives (eg. directive 'root')
// Some details :
// 		\ 'alias' and 'root' do almost the same thing (see <Response.buildFullPath> for difference) :
// 			define the path in the server's file arborescence
// 			at which the file asked requested at this location may be found
// 		\ 'upload' directive defines path to directory in which uploaded files should be stored,
// 			if absent upload is forbidden, if present but empty path is the same as 'root'/'alias'
int parseLineLocation(std::string& line, std::ifstream& inStream, void* storage)
{
	t_location*					location;
	std::vector<std::string>	tokens;
	unsigned int				ind;

	(void)inStream;
	location = reinterpret_cast<t_location*>(storage);
	splitString(line, tokens, " \t");
	if (tokens.size() == 0) // should not happen since <parseBlock> skips empty lines
		return (0);
	else if (tokens[0].compare("return") == 0)
	{
		if (tokens.size() < 2 || location->redirection.size() != 0)
			return (logError("[loca] invalid 'return' directive\n", 0));
		location->redirection = tokens[tokens.size() - 1];
	}
	else if (tokens[0].compare("root") == 0)
	{
		if (tokens.size() != 2 || location->rootPath.size() != 0)
			return (logError("[loca] invalid 'root' directive\n", 0));
		location->rootPath = tokens[1];
	}
	else if (tokens[0].compare("alias") == 0)
	{
		if (tokens.size() != 2 || location->aliasPath.size() != 0)
			return (logError("[loca] invalid 'alias' directive\n", 0));
		location->aliasPath = tokens[1];
	}
	else if (tokens[0].compare("upload") == 0)
	{
		if (tokens.size() > 2 || location->uploadPath.size() != 0)
			return (logError("[loca] invalid 'upload' directive\n", 0));
		if (tokens.size() == 1)
			location->uploadPath = "~";
		else
			location->uploadPath = tokens[1];
	}
	else if (tokens[0].compare("index") == 0)
	{
		if (tokens.size() != 2 || location->defaultFileWhenDir.size() != 0)
			return (logError("[loca] invalid 'index' directive\n", 0));
		location->defaultFileWhenDir = tokens[1];
	}
	else if (tokens[0].compare("autoindex") == 0)
	{
		if (tokens.size() != 2 || location->autoIndexSet)
			return (logError("[loca] invalid 'autoindex' directive\n", 0));
		if (tokens[1].compare("on") == 0)
			location->autoIndex = 1;
		else if (tokens[1].compare("off") == 0)
			location->autoIndex = 0;
		else
			return (logError("[loca] invalid value for 'autoindex' directive\n", 0));
		location->autoIndexSet = 1;
	}
	else if (tokens[0].compare("limit_except") == 0)
	{
		if (tokens.size() < 2 || location->acceptedMethodsSet)
			return (logError("[loca] invalid 'limit_except' directive\n", 0));
		for (ind = 1; ind < tokens.size(); ind++)
		{
			if (tokens[ind].compare("GET") && tokens[ind].compare("POST")
				&& tokens[ind].compare("PUT") && tokens[ind].compare("DELETE")) // #f : adapt to methods really handled
				return (logError("[loca] invalid value for 'limit_except' directive\n", 0));
			location->acceptedMethods.insert(tokens[ind]);
		}
		location->acceptedMethodsSet = 1;
	}
	else if (tokens[0].compare("cgi") == 0)
	{
		if (tokens.size() < 2 || location->cgiExtensions.size() > 0)
			return (logError("[loca] invalid 'cgi' directive\n", 0));
		for (ind = 1; ind < tokens.size(); ind++)
		{
			if (tokens[ind].size() < 2 || tokens[ind][0] != '.')
				return (logError("[loca] invalid value for 'cgi' directive\n", 0));
			location->cgiExtensions.push_back(tokens[ind]);
		}
	}
	else
		return (logError("[loca] unrecognized directive", 0));
	return (0);
}

// Checks if there are incompatibilities in a server block
// 		\ there can't be two location blocks with the same location path
int	invalidVserverParameters(t_vserver& vs)
{
	unsigned int		ind1;
	unsigned int		ind2;

	for (ind1 = 0; ind1 < vs.locations.size(); ind1++)
	{
		for (ind2 = ind1 + 1; ind2 < vs.locations.size(); ind2++)
		{
			if (vs.locations[ind1].locationPath.compare(vs.locations[ind2].locationPath) == 0)
				return (logError("[serv] multiple location blocks with same path is invalid", 0));
		}
	}
	return (0);
}

// Extracts data from line in 'server' block, by either :
// 		\ extracting data from a directive at server level (eg. directive 'listen')
// 		\ calling <parseBlock> to parse a 'location' block,
// 			then checking location validity and adding it to locations list
// Some details :
// 		\ 'listen' directive must at least contain port number, but interface IP is optional
// 			(validity of port number checked by our function, validity of IP checked by <inet_aton>)
// 		\ 'error page' directive must at least contain the path to the error page (last word)
// 			and 1 (or more) error codes leading to that error page
int parseLineServ(std::string& line, std::ifstream& inStream, void* storage)
{
	t_vserver*							vserv;
	t_location							location;
	unsigned int						ind;
	std::vector<std::string>			tokens;
	std::vector<std::string>			tokensListen;
	int									returnCode;
	in_addr								translatedIPAddress;
	t_portaddr 		portaddr;

	vserv = reinterpret_cast<t_vserver*>(storage);
	splitString(line, tokens, " \t");
	if (tokens.size() == 0) // should not happen since <parseBlock> skips empty lines
		return (0);
	if (tokens[0].compare("location") == 0)
	{
		if (tokens.size() != 2)
			return (logError("[serv] invalid 'location' block start", 0));
		initLocation(location);
		location.locationPath = tokens[1];
		if (parseBlock(inStream, "[loca]", &location, &parseLineLocation))
			return (1);
		if (invalidLocationParams(location))
			return (1);
		vserv->locations.push_back(location);
	}
	else if (tokens[0].compare("listen") == 0)
	{
		if (tokens.size() != 2)
			return (logError("[serv] invalid 'listen' directive", 0));
		splitString(tokens[1], tokensListen, ":");
		if (tokensListen.size() < 1 || tokensListen.size() > 2
			|| invalidPortNumber(tokensListen[tokensListen.size() - 1]))
			return (logError("[serv] invalid value for 'listen' directive", 0));
		portaddr.second = static_cast<uint16_t>(strtol(tokensListen[tokensListen.size() - 1].c_str(), NULL, 10));
		if (tokensListen.size() == 2)
		{
			if (inet_aton(tokensListen[0].c_str(), &translatedIPAddress) == 0)
				return (logError("[serv] invalid value for 'listen' directive", 1));
			portaddr.first = ntohl(translatedIPAddress.s_addr);
		}
		else
			portaddr.first = INADDR_ANY;
		addPortaddrToVserv(*vserv, portaddr);
		vserv->listenSet = 1;
	}
	else if (tokens[0].compare("client_max_body_size") == 0)
	{
		if (tokens.size() != 2 || vserv->maxRequestBodySize != std::numeric_limits<long>::max())
			return (logError("[serv] invalid 'client_max_body_size' directive", 0));
		vserv->maxRequestBodySize = strtol(tokens[1].c_str(), NULL, 10);
	}
	else if (tokens[0].compare("error_page") == 0)
	{
		if (tokens.size() < 3)
			return (logError("[serv] invalid 'error_page' directive", 0));
		for (ind = 1; ind < tokens.size() - 1; ind++)
		{
			if (invalidReturnCode(tokens[ind]))
				return (logError("[serv] invalid value for 'error_page' directive", 0));
			returnCode = static_cast<int>(strtol(tokens[ind].c_str(), NULL, 10));
			vserv->errorPages.insert(std::pair<int, std::string>(returnCode, tokens[tokens.size() - 1]));
		}
	}
	else if (tokens[0].compare("server_name") == 0)
	{
		if (tokens.size() < 2 || vserv->serverNames.size() != 0)
			return (logError("[serv] invalid 'server_name' directive", 0));
		for (ind = 1; ind < tokens.size(); ind++)
			vserv->serverNames.insert(tokens[ind]);
	}
	else if (tokens[0].compare("root") == 0)
	{
		if (tokens.size() != 2 || vserv->rootPath.size() != 0)
			return (logError("[serv] invalid 'root' directive", 0));
		vserv->rootPath = tokens[1];
	}
	else
		return (logError("[serv] unrecognized directive", 0));
	return (0);
}

// Extracts data from line in 'http' block
// Since for Webserv everything is forbidden in 'http' block except 'server' blocks,
// this function can just call <parseBlock> to parse 'server' blocks
// 		then checks if the resulting virtual server has no errors, has somewhere to listen on,
// 		and finally adds it to the list of virtual servers
int parseLineHttp(std::string& line, std::ifstream& inStream, void* storage)
{
	std::vector<t_vserver>*	vservers;
	t_vserver				vserv;

	vservers = reinterpret_cast<std::vector<t_vserver>*>(storage);
	if (line.compare("server") == 0)
	{
		initVserver(vserv);
		if (parseBlock(inStream, "[serv]", &vserv, &parseLineServ))
			return (1);
		if (vserv.listenSet == 0)
			vserv.portaddrs.insert(t_portaddr(INADDR_ANY, 80));
		if (invalidVserverParameters(vserv))
			return (1);
		vservers->push_back(vserv);
	}
	else
		return (logError("[http] unrecognized directive", 0));
	return (0);
}

//////////////////////
// PARSING BY BLOCK //
//////////////////////

// Performs the common operations for parsing a block in the cfg file :
// 		\ check that block begins and ends with braces
// 		\ read lines one by one, trim them, sort out empty lines
// 		\ send each non-empty line to function <lineParser> given in parameter
// 				that will extract data from the line, depending on block type ('http/server/location')
// Function <lineParser> must store extracted data in a structure,
// so this structure is given as parameter thanks to a void* pointer, its exact type is :
// 		\ at block 'http' : vector of <t_vserver>
// 		\ at block 'server' : structure <t_vserver>
// 		\ at block 'location' : structure <t_location>
int	parseBlock(std::ifstream& inStream, std::string blockName,
	void* storage, int (*lineParser)(std::string&, std::ifstream&, void*))
{
	std::string				line;
	bool					inBlock = 1;

	if (!blockOpened(inStream, blockName))
		return (logError(blockName + " invalid syntax - braces", 0));
	while (inBlock && std::getline(inStream, line))
	{
		trimString(line);
		//std::cout << blockName + " Line : --" << line << "--\n";
		if (line.size() == 0)
			continue;
		if (line.compare("}") == 0)
			inBlock = 0;
		else
		{
			if (lineParser(line, inStream, storage))
				return (1);
		}
	}
	if (inBlock)
		return (logError(blockName + " invalid syntax - braces", 0));
	return (0);
}

// Opens the configuration file and begins its parsing,
// 		storing extracted information into <t_vserver> structs added to vector <vservers>
// Cfg file must follow the same formatting as an NGINX cfg file, with stricter syntax :
// 		\ only 1 directive per line <=> linebreak is directive delimiter
// 			-> no need for NGINX directive delimiter ';'
// 		\ braces alone on their own line, like in the 42 Norm for C
// 			(eg. a 'server' block would be "server\n{\ndirective;\n}\n")
// Cfg file is structured by blocks that contain either directives or other blocks,
// cfg files for NGINX are much more detailed but for Webserv we only need this simple hierarchy :
// 		cfg file	-> block http	-> block server		-> directive
// 														-> block location		-> directive
// <=> at the level of the cfg file, a directive or a block other than 'http' is forbidden,
// 	   at the level of the 'http' block, a directive or a block other than 'server' is forbidden
// This function reads lines at the cfg file level, so it accepts only 'http' blocks
// 		and lets <parseBlock> do the parsing of the 'http' block
int	parseCfgFile(std::string& cfgFileName, std::vector<t_vserver>& vservers)
{
	std::ifstream			inStream;
	struct stat				fileInfos;
	std::string				line;

	std::cout << "<<< PARSING CONFIG FILE " << cfgFileName << "\n";
	if (stat(cfgFileName.c_str(), &fileInfos) != 0 || !(fileInfos.st_mode & S_IFREG))
		return (logError("Config file not found or invalid", 0));
	inStream.open(cfgFileName.c_str());
	if (!inStream)
		return (logError("Failed to open config file", 1));
	while (std::getline(inStream, line))
	{
		trimString(line);
		//std::cout << "[base] Line : --" << line << "--\n";
		if (line.size() == 0)
			continue;
		if (line.compare("http") == 0)
		{
			if (parseBlock(inStream, "[http]", &vservers, &parseLineHttp))
				return (1);
		}
		else
			return (logError("[base] unrecognized directive", 0));
	}
	std::cout << ">>>\n";
	inStream.close();
	return (0);
}
