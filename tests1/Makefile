# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: ilevy <ilevy@student.42.fr>                +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/04/26 15:17:49 by ilevy             #+#    #+#              #
#    Updated: 2025/04/26 15:21:17 by ilevy            ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME = webserv

SRCS_FOLDER = cpp_files/
SRCS_OBJECTS = utils.cpp Server.cpp Socket.cpp main.cpp
SRCS = $(addprefix $(SRCS_FOLDER), $(SRCS_OBJECTS))
OBJS = $(SRCS:.cpp=.o)
INCLUDE = hpp_files/

CFLAGS = -Wall -Wextra -Werror -std=c++98
CC = clang++

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -I $(INCLUDE) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

names:
	echo $(SRCS) $(OBJS)
