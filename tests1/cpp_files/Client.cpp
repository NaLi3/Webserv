/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilevy <ilevy@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 15:09:41 by ilevy             #+#    #+#             */
/*   Updated: 2025/04/28 17:14:00 by ilevy            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Webserv.hpp"

Client::Client( int socket_fd )
{
	this->_socket_fd = socket_fd;
	if (LOGSV == 1)
		std::cout << "[LOGSV]/ Constructor called for object: Client, socket_fd " << this->_socket_fd << std::endl;
	this->_request_complete = false;
	this->_response_sent = false;
	this->_bytes_sent = 0;
	fcntl(_socket_fd, F_SETFL, O_NONBLOCK);
}

Client::~Client( void )
{
	if (LOGSV == 1)
		std::cout << "[LOGSV]/ Destructor called for object: Client, socket_fd " << this->_socket_fd << std::endl;
	this->closeConnection();
}

bool	Client::readFromSocket( void )
{
	char buffer[4096];
	ssize_t bytes_read;

	bytes_read = recv(this->_socket_fd, buffer, sizeof(buffer), 0);
	if (bytes_read == -1)
	{
		if (errno == EWOULDBLOCK || errno == EAGAIN)
		{
			if (LOGSV == 1)
				std::cout << "[LOGSV]/ No content to be read from socket " << this->_socket_fd << std::endl;
			return (true);
		}
		if (LOGSV == 1)
			std::cout << "[LOGSV]/ Something went wrong with reading from socket " << this->_socket_fd << std::endl;
		return (false);
	}
	else if (bytes_read == 0)
	{
		if (LOGSV == 1)
			std::cout << "[LOGSV]/ Cannot read from socket " << this->_socket_fd << " because connection has been closed" << std::endl;
		return (false);
	}
	else if (bytes_read > 0)
	{
		if (LOGSV == 1)
			std::cout << "[LOGSV]/ Read " << bytes_read << " bytes from socket " << this->_socket_fd << std::endl;
		this->_request_buffer.append(buffer, bytes_read);
		//Entre les headers de la requete http et le corpus optionnel contenu dans la requete, il y a un double retour a la ligne. Je cherche ce double retour pour verifier la fin du message.
		if (_request_buffer.find("\r\n\r\n") != std::string::npos)
			this->_request_complete = true;
		return (true);
	}
}

bool	Client::writeToSocket()
{
	if (this->_response_sent)
		return (true);
	ssize_t bytes;

	bytes = send(this->_socket_fd,
		this->_response_buffer.c_str() + this->_bytes_sent,
		this->_response_buffer.size() - this->_bytes_sent, 0);
	if (bytes > 0)
	{
		this->_bytes_sent += bytes;
		if (this->_bytes_sent >= this->_response_buffer.size())
			this->_response_sent = true;
		return (true);
	}
	else
	{
		if (errno = EWOULDBLOCK || errno == EAGAIN)
			return (true);
		return (false);
	}
}

void	Client::closeConnection( void )
{
	if (this->_socket_fd != -1)
	{
		close(this->_socket_fd);
		this->_socket_fd = -1;
	}
}

int		Client::getSocketFd( void ) const
{
	return (this->_socket_fd);
}

const std::string& Client::getRequestBuffer( void ) const
{
	return (this->_request_buffer);
}

void	Client::setResponse( const std::string& response )
{
	this->_response_buffer = response;
}
