/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: administyrateur <administyrateur@studen    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 15:09:46 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/11 11:27:08 by administyra      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../hpp_files/webserv.hpp"

int 			LOGSV = 1;
volatile int	g_global_signal;

// Modifies the global communication variable to indicate that a SIGINT was received
// to allow the <Server> instance to exit gracefully
void	handleSigint(int sigNum)
{
	if (sigNum == SIGINT)
		g_global_signal = 1;
}

int	main(int ac, char **av)
{
	(void)ac;
	(void)av;
	g_global_signal = 0;
	std::signal(SIGINT, &handleSigint);

	Server		serv;
	if (serv.initServer("webserv.conf")) // #f : make this an argument of the program
		return (1);
	if (serv.startServer())
		return (1);
	if (serv.serverLoop())
		return (1);
	std::cout << "Program exiting\n";

	return (0);
}

/*

/////////////////////
// TCP CONNECTIONS //
/////////////////////

- organisation of internet connections according to TCP protocol :
	TCP protocol is "connection-based" instead of just "IP-based" :
		it identifies server-client connections with a "quad" `ServerIP-ServerPort-ClientIP-ClientPort`
	  -> as long as 2 connections differ by at least 1 element in the quad, they are different connections
			eg. there can be 2 different connections between the same server and client,
			and using the same server port, as long as they are distinguished by different client ports
- how the sockets created by webserv process result is TCP connections on our device :
	\ (level of webserv process) creating a main socket and binding it to an IPaddress+port
	  <=> (level of device) creating an internet TCP connection using the specified IPaddress+port
			-> this connection is in state LISTENING for incoming requests
					(it does not have a fully defined quad, only the parts `ServerIP-ServerPort` are defined)
			   visible in the list of current connections with command `netstat -ntpl`
	\ (level of webserv process) accepting an incoming request of our main socket, and creating a new socket dedicated to it
	  <=> (level of device) creating a new internet connection with the same `ServerIP-ServerPort` as the LISTENING connection
	  		-> this connection is in state ESTABLISHED with a known client counterpart
					(it has a fully defined "quad", that differs on at least 1 part from the quads of all other TCP connections)
			   visible in the list of current connections with command `netstat -ntp`
			/!\ the connection corresponding to the main socket is not altered, it remains active in LISTENING state
- so after connections to multiple clients are accepted by webserv,
	there will be 1 LISTENING connection (<> original main socket) + several ESTABLISHED connections (<> dedicated sockets)
		all with the same "quad" parts `ServerIP-ServerPort`
	then as a client sends a packet to server, it uses this `ServerIP-ServerPort` as destination,
		our OS receives the packets, checks the `ClientIP-ClientPort` part of the quad
		to determine which server-client connection is involved,
			and redirects the packet to the correct dedicated socket
				(or to the main socket if the client is new and has yet to be assigned a dedicated socket)
- /!\ denomination precision : in the webserv program, sockets dedicated to one client
 	are obviously sockets belonging to the server side (we do not control anything on the client side)
		but they are called <clientSocket> for convenience, to distinguish them from the original main socket
- precise roll-out of TCP protocol between server and ONE client has multiple steps,
	where the two sides alternately send packets with flags like (mostly) SYN/ACK/FIN to coordinate their data exchange
		notably at the beginning ("3-way handshake" and HTTPS certificate checks) and at the end (FIN flag)
- in particular the ending dialogue creates a difficulty if our server is the side that closes the connection :
	1. server sends a packet with flag FIN+ACK (connection termination started on server side),
	2. client receives it and answers with a ACK packet then a FIN+ACK packet (termination started on client side),
	3. server receives them and server sends a last ACK packet (server acknowledges that it saw client terminating)
  but then server's TCP connection cannot "die", because of potential problems that the TCP standard MUST handle
	\ the last ACK packet could be lost, so the client would re-send its FIN+ACK packet and expect an answer
	\ there could be "wandering duplicates" of packets belonging to this connection that are lost on some router,
	  	and which have to be properly received by THIS connection so that they don't bother future connection on the same port
  so, as a precaution, server's TCP connection is "kept alive" in TIME_WAIT state for some amount of time (usually 60secs)
	which continues to use the port, so relaunching webserv after it terminated would fail with error "port still in use" on <bind> call
		(/!\ this happens only if the server initiated the closing of the connection,
			if the client sent FIN+ACK first then the client will have to TIME_WAIT while server closes its connection instantly)
	-> this is not a bug, but a normal behavior ; may be avoided by specifying option SO_REUSEADDR on main socket before call to <bind>
		so that <bind> will take possession of the given port even if there is still a connection in TIME_WAIT state using it
			(the connections of the new program will coexist with the old connection in TIME_WAIT state,
				the only constraint is the new program won't be able to create a connection with exactly the same "quad" as the old connection)

////////////////////
// CODE STRUCTURE //
////////////////////

- role of each class :
	\ Server : reads the config file to create (and manage, using function <poll> and instances of Client)
		the main sockets listening for new connections and the dedicated sockets communicating with a client
	\ Client : reacts to the POLLIN (POLLOUT) events received on a dedicated socket
		by reading (writing) into the socket and creating the instances of Request/Response for each request
	\ Request : parses a HTTP request, and does nothing else (therefore, it has no refs to vservers list)
	\ Response : generates the response to a given instance of Request, by
		~ identifying at which location of which virtual server the Request is directed
		~ searching this device's arborescence for files / executing CGI scripts
		~ creating the HTTP response (status code, headers, body)

- handle multiple "virtual" servers :
	\ by parsing cfg file, we have a set of ports+address on which to read
	  and a representation of all "virtual" servers by a list of structs (?)
	\ we setup a main socket FOR EACH different IPaddress+port ;
	  when it receives incoming connection we PEEK (not removing request content from reading queue)
	  into the request to determine to which "virtual" server the request is directed,
	  	according to the port+address of reception and the domain name of request
	\ each ClientHandler instance has the index of its "virtual" server,
		so events from dedicated sockets (ie. from clients already connected)
		are directed to the "virtual" server of their ClientHandler, no need of further checks
- processing of GET requests according to URI :
	\ if ending without a '/' <=> a file with a file extension, serve the resource if it exists
		/!\ redirections / index files defined in conf file may cause a recurrence on another resource URI
	\ if ending with a '/' <=> a directory : search for an 'index.html' file at this location
		if there is none and autoindex option is set generate the index, otherwise return 404

- specific names chosen for clarity :
	\ "portaddr" = a pair constituted by an IP address (int32, potentially 0 for 'any') and a port number (int16)
		representing an interface and a port to which a socket can listen
	\ "client" = instances of the class "Client", with an assigned socket,
		handling all the traffic on that socket (<=> all the communication with 1 client)
	\ "main socket" = a socket that will always be listening without maintaining any TCP connection
	  "dedicated socket" = the socket maintaining TCP connection with a client in particular
  		-> an incoming connection from a client is received on main socket,
			and the call to <accept> creates the dedicated socket that will communicate with client through TCP
		/!\ the server polls both main and dedicated sockets, but events on dedicates sockets are handled
			by the corresponding instance of <Client>

//////////
// MISC //
//////////

- differences apres la mise en commun :
	\ split des methodes de Response entre deux fichiers, un pour les requetes CGI et un pour les autres
		(il y aura sans doute 3 fichiers a terme vu le travail que va demander l'upload)
	\ deplacement des methodes <extractFromURL> et <extractFromHost> dans la classe Request,
		pour qu'elle soit chargee de tout le travail de parsing (et de celui-la seulement)
	\ passage des attributs de Request en public pour etre plus facilement accessibles :
		ca me semble justifiable vu que c'est une classe de parsing et non de logique de serveur
	\ Client recoit les requetes dans une std::string <_requestHeaders> jusqu'a la fin des headers,
		puis cree une instance de Request qui allouera un char* <_body> pour stocker la suite de la requete (le body)
	\ Client envoie ses reponses a partir d'un char* qui a ete alloue par une instance de Response qu'il a creee
	\ dans la gestion de GET/POST/DELETE sans CGI, la recherche du chemin vers le fichier et les autres checks
		ont ete deplaces dans plusieurs methodes pour que ce soit plus lisible
- a adapter dans le fichier de conf pour tester en local :
	\ modifier l'IP sur laquelle ecoute le second serveur (comqnde `ip a` pour IP privee de l'appareil)
	\ modifier les path definis dans les directives 'alias'/'root'
- lignes de tests pour les differentes requetes, avec `nc` :
	\ GET (non-CGI) : ```printf "GET /images/truc.html HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc 127.0.0.1 8080```
	\ POST (non-CGI) : ```printf "POST /images/myfile.txt HTTP/1.1\r\nHost:localhost:8080\r\nContent-Length:3\r\nContent-Type:text/plain\r\n\r\nabc" | nc 127.0.0.1 8080```
	\ DELETE : ```printf "DELETE /images/myfile.txt HTTP/1.1\r\nHost:localhost:8080\r\n\r\n" | nc 127.0.0.1 8080```
	\ POST (CGI) : ```printf "POST /app/handleform.py HTTP/1.1\r\nHost:localhost:8080\r\nContent-Length:17\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\nname=aa&surname=b" | nc 127.0.0.1 8080```

//////////
// TODO //
//////////

+ build true server loop with <poll>
+ test some <write> in a socket (see results with curl/nc)
+ catch signals to shutdown server cleanly (close all sockets and return)
+ parsing of config file (always define a default when a directive is absent)
	\ set port and name of each (virtual) server : `listen address:port`, `server_name name`
		/!\ server name can be an IP
	\ set default error pages : `error_page errcode page_uri`
	\ set maximum allowed size for client request bodies : `client_max_body_size size`
		/!\ size expressed in some defined unit, or accept characters 'K/M/G' for units ?
	\ set and configure routes
		~ define route prepended to a certain requested path : `location path {root prepend}` -> true path is prepend/path
		~ define accepted HTTP methods for a route : `limit_except method` inside location
		~ define HTTP redirect (code 301) : `location oldpath {return 301 newpath}`
	\ enable|disable directory listing : `autoindex on|off` inside location
	\ define a default file to serve when request ends in '/' : `index filename` in location
	\ define file extensions for which CGI should be executed, for POST and GET : custom
	\ allow uploading files with POST/PUT methods, define storage location : custom
- parsing of requests GET/POST/DELETE
- handling of CGIs
- correct handling of errors (cleaning/closing sockets when necessary)
	\ POLLERR or a read failure on a dedicated socket : should close this socket only, not whole server
	\ all cases where a HTTP request (or response generated by CGI) is wrongly formatted
	\ all cases where config file has errors (eg. 0 virtual servers defined)
- handle chunked requests and responses, limit max request size, handle non-text files as entry

+ remplacer les pair<int,short> par un type portaddr, avec un typedef
- tests
	\ faire quelques tests avec nginx pour voir le contenu des headers
	\ tests avec Firefox/curl/nc + eventuellement un script de test
	\ essayer d'utiliser wireshark
- verifier que le vserver selectionne par defaut est bien le premier vserver de la liste
	(depend du choix de container vector/set dans le t_mainSocket)
- en local a 42, utiliser le nom du poste a la place de localhost pour simuler un nom de serveur
- lire le salon webserv pour des tests de charge
- verifier les autorisations sur les fonctions de C et C++ (malloc, free, strdup, bzero...)
	-> sur les erreurs de malloc : faire un petit try catch global pour dire quùon ne crash pas sur erreur de malloc
		(verifier que la structure par classe avec des destructeurs qui nettoient
		 permet de tout bien free meme si on ne le fait pas manuellement)
- rendre clair dans le code ce qui est une erreur fatale ou non
- faire en sorte que les malformed request close la connexion
	(probablement un bool dans Client qui demande de close la connexion quand l'envoi est fini plutot que de passer en read,
		et donc un check du retour de response dans Server pour detruire l'instance une fois qu'elle a close la connexion)
- commentaire sur le fait qu'une malformed request avec un body mais pas de "content-length" va desynchroniser la connexion
- verifier tous les champs du fichier de config et leur implementation (ex. index.html, max body size)
- implementer la reception des chunked requests, verifier si on doit/peut implementer des chunked response
- mecanisme de protection contre plusieurs clients qui chargent des gros fichiers en meme temps
	(exemple : un compteur global du nombre d'octets de memoire heap utilisee)
- actuellement la reponse est ecrite en une fois dans le socket si possible,
	verifier si on devrait definir une taille de buffer de sortie
- verifier le fonctionnement des autres methodes d'envoi de donnees de formulaire (querystring, formulaire)
- verifier si il y a un truc a gerer avec les headers composes de plusieurs champs separes par des ';'
- gerer le souci de determination du path : si on ne trouve pas de '.' pour l'extension, le dernier element est le filename

- verifications sur la gestion des connexions avec le client :
	\ conditions de fin de requête = "\r\n\r\n" et pas de content length,
		ou bien "\r\n\r\n" et content length et autant de bytes dans le body que spécifié par content length
	\ possibilité de "chunked requests" dans HTTP1.1 : requis pqr le sujet,
		ou non requis et on peut répondre "501 - not impl" ?
	\ possibilité de "pipelining" par le client, <=> send plusieurs requêtes sans attendre de réponse à la première
		-> solution : après le read, quand on vérifie si ona une condition de fin de requête,
			aussi vérifier s'il y a le début d'une autre requête dans le buffer et séparer tout ça
				(on reviendra au traitement de la prochaine requête quand on aura envoyé la réponse à la première)
		/!\ implémentation ressemblera un peu à la démarche de gnl de check s'il y a un '\n' dans le remainder
			avant de read à nouveau BUFFER_SIZE caractères (sauf que notre condition de fin de requête est plus complexe que '\n')
*/
