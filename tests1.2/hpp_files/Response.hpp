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

class Response
{
	private:
		Request&		_request;
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

		void			extractExtension();


	public:
		Response( Request& request, t_mainSocket& mainSocket, std::vector<t_vserver>& vservers, std::set<int>& vservIndexes );
		~Response( void );
		int				produceResponse(void);

		std::string&	getResponseBuffer(void);

};

#endif
