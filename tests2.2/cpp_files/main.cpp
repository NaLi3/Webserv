/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilevy <ilevy@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 15:09:46 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/16 18:06:28 by ilevy            ###   ########.fr       */
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
- lignes de test pour un body chunked, avec `nc` :
	\ POST (non-CGI) : ```printf "POST /images/myfile.txt HTTP/1.1\r\nHost:localhost:8080\r\nContent-Type:text/plain\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nabcde\r\n2\r\nf\n\r\n4\r\npol\n\r\n0\r\n\r\n" | nc 127.0.0.1 8080```
	\ POST (CGI) : ```printf "POST /app/handleform.py HTTP/1.1\r\nHost:localhost:8080\r\nContent-Type:application/x-www-form-urlencoded\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nna\r\nB\r\nme=antoine&\r\nD\r\nsurname=bedin\r\n0\r\n\r\n" | nc 127.0.0.1 8080```
- lignes de test d'upload, avec `curl` (sur un fichier "lorem3.txt"):
	\ POST (non-CGI) chunked : ```curl -v http://localhost:8080/images/new.txt -H "Transfer-Encoding: chunked" --data-binary @lorem3.txt```
	\ POST (non-CGI) chunked et multipart : ```curl -v http://localhost:8080/images/ -F "file=@lorem3.txt" -H "Transfer-Encoding: chunked"```

- gestion de Transfer-Encoding: Chunked
	\ autoriser l'absence de Content-Length quand le body est chunked
	\ parser le body au fur et a mesure des receptions, dans <Request.appendToBody>
	\ valeur de retour de <Request.appendToBody> pour indiquer si on a fini
		-> y mettre aussi la logique pour les requetes classiques avec Content-Length
	\ update <Request._bodySize> et realloc <Request._body> au fur et a mesure des chunks
		(double la longueur du body a chaque alloc pour eviter les copies trop frequentes)
	\ rappel solutions pour accueillir des donnees de taille inconnue
		~ allocation manuelle et reallocation (GNL naif) -> complexe, beaucoup de copie inutile
		~ allocation par conteneur std::vector<char> -> beaucoup de copie inutile
		~ allocation manuelle dans une liste chainee (GNL orig) -> complexe
		~ enregistrement des donnees dans un fichier temporaire -> tres bien, pas d'utilisation de la RAM !

- sur la gestion des connexions avec le client :
	\ conditions de fin de requête = "\r\n\r\n" et pas de content length,
		ou bien "\r\n\r\n" et content length et autant de bytes dans le body que spécifié par content length
	\ possibilité de "chunked requests" dans HTTP/1.1 : pas de "Content-Length" dans le headers,
		a la place le body est recu par chunks ayant chacun un header avec leur nombre de bytes,
		et la fin du body est indiquee par un chunk de taille nulle
	\ possibilité de "pipelining" par le client, <=> send plusieurs requêtes sans attendre de réponse à la première
		-> implementation : après le read, quand on vérifie si on a une condition de fin de requête,
			aussi vérifier s'il y a le début d'une autre requête dans le buffer et séparer tout ça
				(on reviendra au traitement de la prochaine requête quand on aura envoyé la réponse à la première)
		/!\ implémentation ressemblera un peu à la démarche de gnl de check s'il y a un '\n' dans le remainder
			avant de read à nouveau BUFFER_SIZE caractères (sauf que notre condition de fin de requête est plus complexe que '\n')

- possibilites de redirection et besoin de modifier (a) status code (b) base href pour ressource HTML
	\ CGI termine en redirection locale 
		(a) reponse finale ne doit pas etre statut 3xx, mais exactement le statut
			que l'on repondrait a une requete pour le path donne par CGI
		(b) presence de base href est de la responsabilite de la ressource ciblee
	\ fichier d'index trouve quand la requete cible une ressource qui est un directory
		(a) statut de reponse sera 200 OK
		(b) pas besoin de base href, puisque ce fichier d'index est cense etre a la base du directory indexe
	\ fichier de config requiert 301 sur une certaine location pour une 'client redirection'
		(a) il faut que le statut soit 3xx
		(b) pas besoin de base href puisque le client va refaire une requete avec le bon path

//////////
// TODO //
//////////

=== MAJOR FEATURES
+ build true server loop with <poll>
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
+ parsing of requests GET/POST/DELETE
+ handling of CGIs on POST/GET
+ file uploads through Content-Type "multipart/form-data"
+ serve automatically generated index files when GET for a directory
+ implement chunked requests

=== CORRECTIONS MINEURES
+ remplacer les pair<int,short> par un type portaddr, avec un typedef
+ gerer le souci de determination du path : '.' dans querystring, absence d'extension...
+ verifier que le vserver selectionne par defaut est bien le premier vserver de la liste
+ verifier les autorisations sur les fonctions de C et C++ (malloc, free, strdup, bzero...)
	-> reponse : on a le droit a toutes les fonctions, avec la contrainte de preferer celles C++ a C
+ rendre clair dans le code ce qui est une erreur fatale ou non
	-> reponse : a part les erreurs de system call dans <Server>,
	   aucune erreur (<Client>/<Request>/<Response> n'est fatale, pour conserver le serveur running) 
+ faire en sorte que les malformed request close la connexion
	(un bool dans Client qui demande de close la connexion quand l'envoi est fini plutot que de passer en read,
	 et donc un check du retour de response dans Server pour detruire l'instance une fois qu'elle a close la connexion)
+ commentaire sur le fait qu'une malformed request avec un body mais pas de "content-length" va desynchroniser la connexion
+ verifier tous les champs non implementes du fichier de config
	+ pages d'erreur
	+ max taille de body
	+ methodes acceptees pour une location
	\ redirections 301 (rajouter champ statut force dans <Request>)
+ implementer methode PUT (qui est une copie de POST non-CGI)
+ actuellement la reponse est ecrite en une fois dans le socket si possible,
	verifier si on devrait definir une taille de buffer de sortie
+ verifier le fonctionnement des autres methodes d'envoi de donnees de formulaire (querystring, formulaire)
~ verifier si il y a un truc a gerer avec les headers composes de plusieurs champs separes par des ';'
- tester profondement les cas ou on fait une requete sur '/' (la racine)
- correct handling of errors (cleaning/closing sockets when necessary)
	\ POLLERR or a read failure on a dedicated socket : should close this socket only, not whole server
	\ all cases where a HTTP request (or response generated by CGI) is wrongly formatted
	\ all cases where config file has errors (eg. 0 virtual servers defined)
- passer la gestion de SIGINT dans l'initialisation de <Server>
- check protocol version
+ agrementer le HTML de la default error page
- implementer tous les '#f'
- permettre a la methode DELETE de supprimer des directories
- boucle bizarre sur les error pages : si page pour 404 contient des ressources,
	que le browser nous les demande et qu'on ne les trouve pas non plus, on re-envoie la page 404
- modifier la gestion des error pages pour ne pas passer par une redirection, mais directement makeFileResponse
	(possible sans boucle, puisque makeFileResponse ne peut que erreur 500, et error pages interdites pour 500)

=== EN BESOIN DE TESTS POUSSES
- tests sur l'identification du vserver et de la location :
	\ location : selection de la location avec le plus long locationPath ?
	\ vserver : selection du vserver avec servername identique au hostname
		(utiliser le nom de poste a 42 comme domain name, par exemple `f4r2s1:8080/`)
- tests sur l'execution des CGI : Python et PHP
- tests de charge avec la commande `siege` (man / salon discord pour plus de precisions)
- tests avec le testeur fourni par 42 (requiert gestion de la methode PUT)

=== A ENVISAGER
+ amelioration de l'implementation de la reception de chunked requests :
	actuellement <appendToChunkedBody> attend le "\r\n" pour calculer la taille de chunk
	dans le buffer qu'on lui donne en para, donc problemes potentiels avec petites tailles de buffer
		-> corriger avec un std::string gardant en memoire les morceaux de buffer jusque trouver "\r\n"
- tests avec outils externes
	\ faire quelques tests avec nginx pour voir le contenu des headers,
		surtout des tests sur redirection 301 pour voir si c'est user ou local redirection
	\ eventuellement un script de test utilisant curl/nc ?
	\ essayer d'utiliser wireshark
- protection contre l'epuisement de la memoire dynamique
	\ des try-catch bien places pour englober tous les `new` ?
	\ un compteur de memoire consommee pour refuser les clients quand on atteint une limite ?
- implementation minimale de la gestion du pipelining :
	apres avoir recu des octets 'en trop', les stocker pour y revenir au prochain POLLIN
- lecture dans un socket quand on recoit un POLLIN : actuellement on se limite
  a lire une taille de buffer, puis on attend le prochain POLLIN pour lire a nouveau
	mais pourrait-on lire jusqu'a ce qu'il n'y ait plus rien dans le socket
		(et simplement checker que le premier read n'est pas nul pour la condition de fin de connexion)

=== POSSIBLES AMELIORATIONS POST-CC
- gestion plus precise des erreurs 400 : si <Request.parse> renvoie 1 pour une erreur,
	il ne faut close la connection pour cause de desynchronisation
	QUE SI "Content-Length" est inconnu, sinon on peut toujours ignorer cette requete
	et lire la suivante sans perdre le fil
- de maniere generale, definir plusieurs valeurs de retour pour <Request.parse> :
	malformed request non desynchronisante, malformed request desynchronisante, not implemented...
- amelioration/harmonisation de la recherche de "\r\n\r\n" dans les octets lus dans le socket :
	maintenir un <lastBuffer> avec le dernier buffer lu (contenant potentiellement le debut du sep)
	est redondant puisque on a deja ces octets stockes dans dans la std::string des headers
		-> creer le "combined" on recherche sep a partir des 3 derniers octets de cette string + le buffer
- amelioration de la reception des chunked requests : accepter les 'trailers'
	(les headers ajoutes par le client apres le dernier chunk)
- implementation des chunked responses (le Client, au lieu de recuperer le buffer de sa Response,
	la laisserait ecrire ou non un chunk dans le socket a chaque event POLLOUT)
- gestion des chemins relatifs dans le fichier de configuration :
	quand il y a une directive 'root' dans ce bloc ou un bloc inferieur,
	on peut accepter les chemins ne commencant pas par '/' en les prefixant du chemin 'root'
- replacer la determination de vserver et location dans la classe <Request> :
	c'est plus coherent et necessaire vu le check de maxRequestBodySize du vserver,
	meme si ca s'eloigne un peu du strict role de parsing de <Request>
*/
