NAME = ./webserv
CXX = c++

INCDIRS = ./includes ./includes/legacy
CXXFLAGS = -g -Wall -Wextra -Werror -std=c++98 $(addprefix -I,$(INCDIRS))

SRCS = \
        ./srcs/main.cpp \
        ./srcs/cfg/Config.cpp \
        ./srcs/cfg/ConfigLexer.cpp \
        ./srcs/cfg/ConfigParser.cpp \
		./srcs/cgi/Cgi.cpp \
	./srcs/server/Chunked.cpp \
	./srcs/server/ServerSocket.cpp \
	./srcs/server/SocketManager.cpp \
	./srcs/server/SocketManagerDelete.cpp \
	./srcs/server/SocketManagerHttp.cpp \
	./srcs/server/SocketManagerPost.cpp \
	./srcs/server/Response.cpp \
	./srcs/server/MultipartStreamParser.cpp \
	./srcs/utils/file_utils.cpp \
        ./srcs/utils/request_parser.cpp \
        ./srcs/utils/utils.cpp

OBJS = $(SRCS:.cpp=.o)

LEGACY_SRCS := $(wildcard srcs/legacy/*.cpp)
LEGACY_OBJS := $(LEGACY_SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re