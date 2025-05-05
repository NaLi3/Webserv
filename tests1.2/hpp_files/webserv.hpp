/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abedin <abedin@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 15:19:26 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/05 15:38:14 by abedin           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#ifndef WEBSERV_HPP
# define WEBSERV_HPP

extern int	LOGSV;
// Global variable for communication of signal SIGINT
extern volatile int	g_global_signal;

# define GET 1
# define POST 2
# define PUT 3
# define DELETE 4

// Classes and structures
class	Server;
class	Client;
class	Request;
class	Response;
typedef struct s_vserver t_vserver;
typedef struct s_location t_location;
typedef struct s_mainSocket t_mainSocket;

# include "libraryUsed.hpp"
# include "Client.hpp"
# include "Server.hpp"
# include "Request.hpp"
# include "Response.hpp"
# include "cfgParsing.hpp"

// Misc functions
int		logError(std::string msg, bool displayErrno);
void	logTime(int linebreak);

#endif
