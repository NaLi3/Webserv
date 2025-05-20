/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abedin <abedin@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 15:09:46 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/20 20:53:04 by abedin           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../hpp_files/webserv.hpp"

volatile int	g_global_signal;

int	main(int ac, char **av)
{
	if (ac != 2)
	{
		std::cout << "Usage : `./webserv cfg_file`\n";
		return (1);
	}

	Server		serv;
	if (serv.initServer(av[1]))
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
	\ Request : parser class with public attributes
		~ parsing a HTTP request
		~ identifying at which location of which virtual server the Request is directed
	\ Response : generates the response to a given instance of Request, by
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

/////////////////////////////////////
// SPECIFIC IMPLEMENTATION DETAILS //
/////////////////////////////////////

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

- gestion des buffers recus par <recv> de la connexion avec le client :
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

- possibilites de servir une autre ressource que celle demandee,
  et besoin de definir (a) status code (b) base href pour ressource HTML
	\ CGI resout en redirection LOCALE
		(a) reponse finale ne doit pas etre statut 3xx, mais exactement le statut
			que l'on repondrait a une requete pour le path donne par CGI
		(b) presence de base href est de la responsabilite de la ressource ciblee
	\ servir un fichier d'index quand la ressource demandee est un directory avec trailing '/'
		(a) le statut doit etre 200 OK
		(b) pas besoin du base href puisque le fichier d'index est dans le directory demande par requete
	\ servir une page d'erreur definie par fichier de config
		(a) le statut doit etre celui de l'erreur
		(b) on peut inserer le base href directement dans la page,
			ou laisser la responsabilite aux error pages
- possibilites de redirection CLIENT
	\ CGI resout en redirection CLIENT
	\ fichier de config requiert redirection CLIENT sur une certaine location 
	\ client demande une ressource directory sans mettre le trailing '/'
	  ou une ressource file en mettant le trailing '/'

- gestion de l'execution des CGIs : le process de Webserv fork un enfant charge de `execve` le CGI
	\ probleme de performance : `fork` fonctionne comme s'il copiait tout le processus originel,
		y compris les donnees sans lien avec ce CGI (par ex des données dédiées à d'autres clients) ;
	  si la copie s'effectuait vraiment ce serait tres inefficace puisque tout ce qui est copie par `fork`
	  est ensuite ecrase par `execve`
	  	mais en realite `fork` fait en sorte que process parent et enfant partage la memoire du process originel
			et c'est seulement quand l'enfant veut modifier une zone memoire partagee
			qu'une copie privee (de cette zone memoire uniquement) est cree pour lui ("copy on write", COW)
	  -> pendant `fork`, seulement le "registre des pages memoires" et le "registre des fd" sont copies,
	  	donc moins inefficace que prevu meme si ca peut etre non-trivial en cas de gros process original
	\ alternative : au lieu de fork un process serveur originel deja bien enfle en memoire et fds,
		faire tourner a cote du process serveur un process specialise en CGI beaucoup moins lourd :
			le process serveur envoie les CGIs a executer par un pipe,
			et le process specialise realise le fork (en adaptant les fd I/O comme d'habitude)
		= exactement la solution utilisee par la methode fastCGI, utilisant soit
			~ un process CGI specialise forke au lancement du serveur, ecoutant sur un pipe
			~ un process CGI daemon tournant en permanence, ecoutant sur un socket en boucle interne (ex php-fpm)
		(autre possibilite : fonction recente <posix_spawn> qui combine `fork` et `execve`
			en evitant les copies intermediaires inutiles puisque ecrasees par `execve`)
	\ pour implementation CGI dans Webserv, on peut se contenter de `fork` + `execve` ; entre les deux :
		~ pas besoin de liberer toute la memoire allouee par le process originel, car `execve` l'ecrasera
		~ par contre besoin de fermer tous les fds inutiles (notamment fds de sockets)
		~ et besoin de <chdir> vers le dossier qui contient la ressource executee

- deux manieres possible de gerer l'execution des CGIs :
	(1) (version initiale pour Webserv, abandonnee) quand une requete cible un fichier avec une extension qualifiee CGI,
		on `execve` directement sur ce fichier, qui doit donc soit etre un executable (un binaire)
			soit etre un script (PHP, Python) avec un shebang en premiere ligne donnant l'interpreteur a `execve`
		-> dans ce cas, la variable d'env PATH_INFO doit bien etre la partie de l'URI entre le file path et la query string
	(2) (version finalement implementee pour Webserv) quand une requete cible un fichier avec une extension qualifiee CGI,
		la cfg donne le path vers un interpreteur qui doit etre appele avec `execve`, en donnant le fichier en argument
			<=> l'interpreteur est `argv[0]` de `execve`, le fichier cible par la requete est `argv[1]`
		-> dans ce cas, la variable d'env PATH_INFO doit juste etre le path virtuel vers le fichier cible par la requete
- pourquoi une difference de PATH_INFO entre les deux ? en fait, la maniere (2) est plus ou moins le resultat de la maniere (1)
	quand on le file path mene a l'interpreteur, et le path_info de l'URI mene au script generateur (qui n'a pas besoin de shebang)
		-> effectivement, le PATH_INFO variable d'env de (1) serait alors le path vers le script, comme dans (2)
- le sujet choisit la maniere (2), et donc le PATH_INFO doit bien etre defini comme le path virtuel vers le script
	(et le PATH_TRANSLATED, pas utilise par le CGItest de 42 mais utilise par PHP,
		doit etre le path reel, dans l'arborescence de fichier de l'appareil, menant au script)

////////////////////////////////////
// UNDERSTANDING AND TESTING CODE //
////////////////////////////////////

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

//////////
// TODO //
//////////

=== MAJOR FEATURES
+ build true server loop with <poll>
+ catch signals to shutdown server cleanly (close all sockets and return)
+ parsing of config file (always define a default when a directive is absent)
	+ set port and name of each (virtual) server : `listen address:port`, `server_name name`
		/!\ server name can be an IP
	+ set default error pages : `error_page errcode page_uri`
	+ set maximum allowed size for client request bodies : `client_max_body_size size`
	+ set and configure routes
		~ define route prepended to a certain requested path : `location path {root prepend}` -> true path is prepend/path
		~ define accepted HTTP methods for a route : `limit_except method` inside location
		~ define HTTP redirect (code 301) : `location oldpath {return 301 newpath}`
	+ enable|disable directory listing : `autoindex on|off` inside location
	+ define a default file to serve when request ends in '/' : `index filename` in location
	+ define file extensions for which CGI should be executed, for POST and GET : custom
	+ allow uploading files with POST/PUT methods, define storage location : custom
+ parsing of requests GET/POST/DELETE
+ handling of CGIs on POST/GET
+ file uploads through Content-Type "multipart/form-data"
+ serve automatically generated index files when GET for a directory
+ implement chunked requests
+ implement session data through cookie PHPSESSID

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
	+ redirections 301 (rajouter champ statut force dans <Request>)
+ modifier la gestion des error pages pour ne pas passer par une redirection, mais directement makeFileResponse
	(possible sans boucle, puisque makeFileResponse ne peut que erreur 500, et error pages interdites pour 500)
+ actuellement la reponse est ecrite en une fois dans le socket si possible,
	plutot definir une taille de buffer de sortie
+ verifier le fonctionnement des autres methodes d'envoi de donnees de formulaire (querystring, formulaire)
+ correct handling of errors :
	+ read failure on a dedicated socket : should close this socket only, not whole server
		-> <Client.handleEvent>'s return values 1 (error) and 2 (close conn) must be handle identically 
	+ cases where a HTTP request is wrongly formatted -> connection should be closed
+ passer la gestion de SIGINT dans l'initialisation de <Server>
+ check in <Request.parse> that HTTP protocol version is HTTP/1.1
+ placer le base href dans les error pages definies par le fichier de cfg
+ agrementer le HTML de la default error page
+ implementer tous les '#f'
+ permettre a la methode DELETE de supprimer des directories
+ faire une fonction dans Request qui check qu'un path ne contient pas de fichiers caches / '.' / '..'
+ correction des CGIs
	+ gerer la facon dont le tester comprend les CGIs
	+ gerer le gros test POST en passant l'output CGI dans un fichier temporaire plutot qu'un pipe
	+ changer de directory avant le execve
	+ close tous les fds sauf 0/1/2 avant le execve
	+ gerer les special headers
+ bugs segfault
	+ quand la premiere requete recue est un upload multipart
	+ quand on doit effectuer une redir response a partir d'un path to directory sans '/'
+ ecrire des commentaires dans ResponseUpload
+ limiter le nombre de clients acceptables en simultane (macro MAX_SIMULTANEOUS_CLIENTS)
+ protection try-catch sur les <handleEvent> des clients pour l'epuisement de la heap
+ voir comment eviter de hang sur un script CGI qui pionce

=== EN BESOIN DE TESTS POUSSES
+ petits ajouts pour que le testeur42 fonctionne :
	+ gestion de la methode PUT (comme POST, sans CGI)
	+ cfg accepte les methodes POST dans location '/directory/'
	+ effacement du saut de ligne etrange parfois intercale entre les requetes
	+ ajout du fichier 'youpla.bla' qui doit apparemment etre present
+ tests sur l'identification du vserver et de la location :
	+ location : selection de la location avec le plus long locationPath
	+ vserver : selection du vserver avec servername identique au hostname
		(utiliser curl pour controler le champ 'Host' de la requete)
+ tests de charge avec la commande `siege` (man / salon discord pour plus de precisions)
+ tests sur l'execution des CGI : Python et PHP
+ creer un petit script de test utilisant curl
+ tests ciblant la racine '/'
- tests sur le fichier de config : des erreurs dans les directives, des '{}' incoherents, aucun vserver...

=== A ENVISAGER
+ amelioration/harmonisation de la recherche de "\r\n\r\n" dans les octets lus dans le socket :
	maintenir un <lastBuffer> avec le dernier buffer lu (contenant potentiellement le debut du sep)
	est redondant puisque on a deja ces octets stockes dans dans la std::string des headers
		-> creer le "combined" on recherche sep a partir des 3 derniers octets de cette string + le buffer
+ amelioration de l'implementation de la reception de chunked requests :
	actuellement <appendToChunkedBody> attend le "\r\n" pour calculer la taille de chunk
	dans le buffer qu'on lui donne en para, donc problemes potentiels avec petites tailles de buffer
		-> corriger avec un std::string gardant en memoire les morceaux de buffer jusque trouver "\r\n"
+ replacer la determination de vserver et location dans la classe <Request> :
	c'est plus coherent, et necessaire vu le check de maxRequestBodySize du vserver,
	meme si ca s'eloigne un peu du strict role de parsing de <Request>
+ gestion des chemins relatifs dans le fichier de configuration :
	on peut accepter les chemins ne commencant pas par '/' en les prefixant du pwd
+ ameliorer la solution (transitoire) d'attente de fin d'execution du CGI
	en soumettant le fd sortant du pipe a <pollfd> (qui devrait renvoyer POLLIN quand le pipe se remplit ou close)

=== POSSIBLES AMELIORATIONS POST-CC
- gestion plus precise des erreurs 400 au parsing :
	\ si <Request.parse> renvoie 1 pour une erreur,
		il ne faut close la connection pour cause de desynchronisation
		QUE SI "Content-Length" est inconnu, sinon on peut toujours ignorer cette requete
		et lire la suivante sans perdre le fil
	\ de maniere generale, definir plusieurs valeurs de retour pour <Request.parse> :
		malformed request non desynchronisante, malformed request desynchronisante, not implemented...
- amelioration fondamentale de la gestion de l'envoi/reception des donnes de body :
	\ chunked requests :
		~ accepter les 'trailers' (les headers ajoutes par le client apres le dernier chunk)
		~ ecrire les chunks recus dans un fichier temporaire plutot qu'allouer un buffer
	\ implementation des chunked responses (le Client, au lieu de recuperer le buffer de sa Response,
		la laisserait ecrire ou non un chunk dans le socket a chaque event POLLOUT)
	\ de maniere generale, tous les body ne devraient pas etre ecrits dans des buffers,
		mais transmis directement au fd concerne apres unchunk ou stockes en fichier temporaire
	==> restructuration tres large de tout le flow du serveur pour passer en mode 'streaming' et 'event-driven',
		notamment car le processing des requetes doit alors se faire AVANT la reception du body
- eviter qu'une requete puisse 'hang indefinitely' a cause d'un script CGI buggy
	en fixant un temps maximal d'execution des CGIs (si depasse, kill l'enfant et renvoyer 500)
- gerer proprement la reception des requetes HEAD
	<=> renvoyer la meme reponse que pour GET, mais sans body)
- simplifier/clarifier le calcul et la verification du path vers la ressource
- protection contre l'epuisement de la memoire dynamique
	\ des try-catch bien places pour englober tous les `new` ?
	\ un compteur de memoire consommee pour refuser les clients quand on atteint une limite ?
- implementation minimale de la gestion du pipelining :
	apres avoir recu des octets 'en trop', les stocker pour y revenir au prochain POLLIN

///////////////////////
// BEFORE EVALUATION //
///////////////////////

- changement des IPs pour le vserver operant sur l'IP non boucle locale :
	\ dans le fichier de config
	\ dans testerVservLoc.sh
- utilisation de siege :
	\ -v : verbose mode ; -b : benchmarking mode
	\ -p <URL> : URL to target
	  -f <filename> : file containing list of URLs to target
	\ -c <num> : number of concurrent users to simulate
	\ -r <num> : number of requests per simulated user before dying
	  -t <num + S|M|H> t : time each simulated user does requests before dying
	-> example : `siege -v -b -f urls.txt -c 255 -r 20`

- to explain during correction
	\ difference between current buffering approach and ideal event-driven streaming approach
	\ general structure : cfg file parsing, Server for multiplexing, Request for parsing, Response for answering
- to show during correction
	\ handling multiple connections with 2 different clients (browser on sleeperpage + tester)
	\ handling multiple connections from the same client (browser multiplexing on welcomepage)
	\ handling POST CGIs : methods of form data transmission, execution of Python CGI
	\ handling UPLOAD : raw, multipart/form-data, chunked bodies
	\ handling cookies and session data : see PHPSESSID cookie in our headers, and in browser inspection
	\ handling locations and virtual server : location '/site/secretplace' in main site
											  location '/app/' taking CGI, but no CGI if files accessed by '/programs/' !
											  vserver on other IP / with different name (use curl)

+ tests de location et vserver
+ tests de '/'
- tests de cfg file
- tests buggy (malformed request, forbidden method, forbidden upload, body too large, body not fit)
- page DELETE
- relecture du sujet
- verification tester42
- retrait des logs (+ '#f' et logs des vars d'env dans CGI)
- reverification tester42

*/
