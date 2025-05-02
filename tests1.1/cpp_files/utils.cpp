/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: administyrateur <administyrateur@studen    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 15:09:53 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/02 16:03:41 by administyra      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../hpp_files/webserv.hpp"

int	logError(std::string msg, bool displayErrno)
{
	std::cout << msg << "\n";
	if (displayErrno)
		perror("Error description");
	return (1);
}
