/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abedin <abedin@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 15:48:28 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/05 09:33:12 by abedin           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#ifndef RESPONSE_HPP
# define RESPONSE_HPP

# include "webserv.hpp"

# define MAXBODYSIZE 4096

class Response
{
	private:
		Request&		_request;
		int				_clientSocket;
		t_mainSocket&	_mainSocket;
		t_vserver*		_vserv;
		t_location*		_location;
		std::string		_responseBuffer;

		std::string		_extension;
		std::string		_cgiScript;

		t_vserver*		identifyVserver(std::vector<t_vserver>& vservers, std::set<int>& vservIndexes);
		t_location*		identifyLocation();
		bool			isCGIRequest();
		int				executeCGIScript();
		std::string		getSameType( const std::string& path );
		void			extractExtension();


	public:
		Response( Request& request, int clientSocket, t_mainSocket& mainSocket, std::vector<t_vserver>& vservers, std::set<int>& vservIndexes );
		~Response( void );
		void			handleGet(int clientSocket);
		void			handlePost( int clientSocket );
		void			handleDelete( int clientSocket );
		void			handleError( int clientSocket, int code, const std::string& message );
		int				produceResponse(void);
		std::string&	getResponseBuffer(void);

};

bool ends_with( const std::string& str, const std::string& suffix );

#endif
