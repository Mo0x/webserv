NAME = ./webserv
CXX = c++
SRCS =	./srcs/main.cpp \
		./srcs/cfg/Config.cpp \
		./srcs/cfg/ConfigLexer.cpp \
		./srcs/cfg/ConfigParser.cpp \
		./srcs/server/ServerSocket.cpp \
		./srcs/server/SocketManager.cpp \
		./srcs/utils/request_parser.cpp \
		./srcs/utils/file_utils.cpp \
		./srcs/utils/utils.cpp \
		./srcs/server/Response.cpp

OBJS = $(patsubst %.cpp, %.o, $(SRCS))
INCDIRS = ./includes
CXXFLAGS = -g -Wall -Wextra -Werror -std=c++98 -I$(INCDIRS)

all : $(NAME)


$(NAME) : $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS) 

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^

clean :
	rm -rf $(OBJS)

fclean : clean
	rm -rf $(NAME)

re : fclean all 

.PHONY : all clean fclean re