/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: administyrateur <administyrateur@studen    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/30 15:48:28 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/02 16:36:42 by administyra      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#ifndef REQUEST_HPP
# define REQUEST_HPP

# include "webserv.hpp"

/*Request takes in the data from Client upon verification of Client's 'isComplete()' method.
  If the client read a complete http message, the parse() method of Client's request attribute
  should be called. Parse will check the validity of the HTTP request compliant by the http/tcp
  and return true if compliant, false otherwise. isComplete() in the request object looks for the end of the headers.*/

class Request
{
	private:
		std::string							_method;
		std::string							_path;
		std::string							_httpVersion;
		std::map<std::string, std::string>	_headers;
		std::string							_body;
		bool								_parsed;

		bool	parse_request( const std::string& raw_request, size_t header_end );
		bool	parse_headers( const std::string& raw_request, size_t header_end );


	public:
		Request( void );
		~Request( void );

		bool	parse( const std::string& raw_request );
		bool	isComplete( const std::string& buffer ) const;
		void	log_request(void);

		const std::string& getMethod( void ) const;
		const std::string& getPath( void ) const;
		const std::string& getHttpVersion( void ) const;
		const std::string& getBody( void ) const;
		const std::map<std::string, std::string>& getHeaders() const;
};

#endif
