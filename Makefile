NAME = ./webserv
CXX = c++
SRCS =	./srcs/main.cpp \
		./srcs/server/Server.cpp \
		./srcs/cfg/Config.cpp \
		./srcs/utils/Logger.cpp

OBJS = $(patsubst %.cpp, %.o, $(SRCS))
INCDIRS = ./includes
CXXFLAGS = -g -Wall -Wextra -Werror -std=c++98 -I$(INCDIRS) -pedantic
LDFLAGS = -no-pie

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