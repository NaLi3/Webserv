# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: abedin <abedin@student.42.fr>              +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/04/26 15:17:49 by ilevy             #+#    #+#              #
#    Updated: 2025/05/05 09:41:11 by abedin           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME = webserv

SRCS_FOLDER = cpp_files/
SRCS_OBJECTS = 	utils.cpp main.cpp \
				Client.cpp Server.cpp \
				Request.cpp Response.cpp \
				cfgParsing.cpp
SRCS = $(addprefix $(SRCS_FOLDER), $(SRCS_OBJECTS))
OBJS = $(addprefix $(OBJ_DIR), $(SRCS_OBJECTS:.cpp=.o))
OBJ_DIR = o_files/
INCLUDE = hpp_files/

CFLAGS = -Wall -Wextra -Werror
CC = clang++

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -I $(INCLUDE) $(OBJS) -o $(NAME)
	@echo "Compiled the cpp files into o_files!"
	@echo "Compiled Webserv!"

$(OBJ_DIR)%.o: $(SRCS_FOLDER)%.cpp | objdir_mk
	$(CC) $(CFLAGS) -I $(INCLUDE) -c $< -o $@

objdir_mk:
	@mkdir -p $(OBJ_DIR)

clean:
	@rm -rf $(OBJ_DIR)
	@echo "Cleaned o_files!"


fclean: clean
	@rm -f $(NAME)
	@echo "Cleaned Webserv!"

re: fclean all

names:
	echo $(SRCS) $(OBJS)

.PHONY:	re all fclean clean objdir_mk names
