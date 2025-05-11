/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilevy <ilevy@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 15:09:57 by ilevy             #+#    #+#             */
/*   Updated: 2025/04/28 17:14:00 by ilevy            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <string>
# include <fstream>
# include <iostream>
# include <stdlib.h>
# include <sys/socket.h>
# include <fcntl.h>
# include <unistd.h>
# include <errno.h>

class Client
{
	private:
		int			_socket_fd;
		std::string	_request_buffer;
		std::string	_response_buffer;
		bool		_request_complete;
		bool		_response_sent;
		size_t		_bytes_sent;

	public:
								Client( int socket_fd );
								~Client( void );

		bool					readFromSocket( void );
		bool					writeToSocket( void );
		void					closeConnection( void );

		int						getSocketFd( void ) const;
		const	std::string&	getRequestBuffer( void ) const;
		void					setResponse( const std::string& response_msg );
};

#endif
