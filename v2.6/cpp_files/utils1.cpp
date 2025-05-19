/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils1.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abedin <abedin@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 15:09:53 by ilevy             #+#    #+#             */
/*   Updated: 2025/05/16 18:01:26 by abedin           ###   ########.fr       */
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

// Frees each array of characters in <arr>, then <arr> itself
void	free2dimArray(char** arr, int size)
{
	int		ind;

	ind = 0;
	while (ind < size)
	{
		delete[] arr[ind];
		ind++;
	}
	delete[] arr;
}

// Modifies the global communication variable to indicate that a SIGINT was received
// to allow the <Server> instance to exit gracefully
void	handleSigint(int sigNum)
{
	if (sigNum == SIGINT)
		g_global_signal = 1;
}
