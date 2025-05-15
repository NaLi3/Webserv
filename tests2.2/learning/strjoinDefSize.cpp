#include <cstring>
#include <iostream>

char*	ft_strjoinDefsize(char* s1, char* s2, size_t sizes2)
{
	char*		res;
	size_t		ind;
	size_t		sizes1;

	sizes1 = 0;
	while (s1[sizes1])
		sizes1++;
	res = new char[sizes1 + sizes2 + 1];
	res[sizes1 + sizes2] = '\0';
	for (ind = 0; ind < sizes1; ind++)
		res[ind] = s1[ind];
	for (; ind < sizes1 + sizes2; ind++)
		res[ind] = s2[ind - sizes1];
	return (res);
}

void	printAll(char* s, size_t size)
{
	size_t	ind;

	for (ind = 0; ind < size; ind++)
		std::cout << " -" << s[ind] << "- ";
	std::cout << "\n";
}

int	main(int ac, char **av)
{
	char	*s3;
	size_t	sizes2;

	if (ac < 3)
		return (1);
	sizes2 = strlen(av[2]);
	std::cout << "s1 " << av[1] << "; s2 " << av[2] << "\n";
	av[1][3] = '\0';
	av[2][3] = '\0';
	std::cout << "s1 " << av[1] << "; s2 " << av[2] << "\n";
	s3 = ft_strjoinDefsize(av[1], av[2], sizes2);
	std::cout << "s3 " << s3 << "\n";
	printAll(s3, strlen(av[1]) + sizes2);
	return (0);
}