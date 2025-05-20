#pragma once

#ifndef UTILSTEMPLATES_H
# define UTILSTEMPLATES_H

# include "webserv.hpp"

// Returns the given argument as a string literal
// using the method of putting it into a string stream, then extracting it
// (designed for numeric type, but it works for any type
//  that has an overload defined for operator '<<' insertion into a string stream)
template <class T>
std::string		itostr(T number)
{
	std::stringstream	ss;
	std::string			numberStr;

	ss << number;
	numberStr = ss.str();
	return (numberStr);
}

#endif