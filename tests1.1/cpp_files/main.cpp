/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilevy <ilevy@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 15:09:46 by ilevy             #+#    #+#             */
/*   Updated: 2025/04/28 21:51:06 by ilevy            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <string>
#include <iostream>
#include "../hpp_files/Webserv.hpp"


int	main(int ac, char **av)
{
	Server		serv("cfg");
	Client		client()


	(void)ac;
	(void)av;
	if (serv.startServer())
		return (1);
	if (serv.waitForConnection())
		return (1);

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



- essayer d'utiliser wireshark
- verifier comment catch les signaux pour toujours shutdown le serveur proprement

*/
