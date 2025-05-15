/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils1.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: administyrateur <administyrateur@studen    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 15:09:53 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/09 15:50:21 by administyra      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../hpp_files/webserv.hpp"

// Displays a user-defined error message <msg>,
// and the errno message if <displayErrno> is true
int	logError(std::string msg, bool displayErrno)
{
	std::cerr << msg << "\n";
	if (displayErrno)
		perror("Error description");
	return (1);
}

// Displays the current hour as HH:MM:SS
void	logTime(int linebreak)
{
	time_t				timestamp = time(NULL);
	struct tm*			datetime;

	datetime = localtime(&timestamp);
	if (datetime->tm_hour < 10)
		std::cout << "0";
	std::cout << datetime->tm_hour << ":";
	if (datetime->tm_min < 10)
		std::cout << "0";
	std::cout << datetime->tm_min << ":";
	if (datetime->tm_sec < 10)
		std::cout << "0";
	std::cout << datetime->tm_sec;
	if (linebreak)
		std::cout << "\n";
	else
		std::cout << " ";
}

// Frees each array of characters in <arr>, then <arr> itself.
// /!\ Custom function adapted to the specific case where <arr> was allocated using <new>,
// but its elements were allocated using <malloc> (through <strdup>)
// 		-> do not use elsewhere
void	free2dimArray(char** arr, int size)
{
	int		ind;

	ind = 0;
	while (ind < size)
	{
		free(arr[ind]);
		ind++;
	}
	delete[] arr;
}