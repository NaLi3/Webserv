/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ilevy <ilevy@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 15:09:53 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/06 14:24:31 by ilevy            ###   ########.fr       */
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

void	logTime(int linebreak)
{
	time_t				timestamp = time(NULL);
	struct tm*			datetime;

	datetime = localtime(&timestamp);
	std::cout << datetime->tm_hour
		<< ":" << datetime->tm_min
		<< ":" << datetime->tm_sec;
	if (linebreak)
		std::cout << "\n";
	else
		std::cout << " ";
}
