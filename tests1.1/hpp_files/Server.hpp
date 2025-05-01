/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilevy <ilevy@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 15:10:01 by ilevy             #+#    #+#             */
/*   Updated: 2025/04/30 15:49:39 by ilevy            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_H
# define SERVER_H

# define MAX_QUEUED_CONNECTIONS 10

class	Server;

class	Server
{
	public:
		Server(std::string cfgFileName);
		~Server();
		int			startServer(void);
		int			waitForConnection(void);
		int			handleClientRequest(int clientSocketFd);
		int			getSocketFd( void ) const;
	private:
		uint16_t	_port;
		std::vector<Client*> clientList;
		int			_mainSocketFd;
};

int	logError(std::string msg, bool displayErrno);

#endif
