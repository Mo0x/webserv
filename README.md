Project Path: webpost

Source Tree:

```txt
webpost
├── Dev_A
├── Dev_B
├── Functions
├── Makefile
├── TODOmax
├── TODOupdated
├── Webserv_Project_Task_Checklist.csv
├── config.conf
├── configlocationtest.conf
├── cursus
├── includes
│   ├── CGIHandler.hpp
│   ├── Config.hpp
│   ├── ConfigLexer.hpp
│   ├── ConfigParser.hpp
│   ├── FileUploadHandler.hpp
│   ├── Logger.hpp
│   ├── MultipartParser.hpp
│   ├── Server.hpp
│   ├── ServerSocket.hpp
│   ├── SocketManager.hpp
│   ├── file_utils.hpp
│   ├── request_parser.hpp
│   ├── request_reponse_struct.hpp
│   ├── to_delete_ServerConfig.hpp
│   └── utils.hpp
├── srcs
│   ├── cfg
│   │   ├── Config.cpp
│   │   ├── ConfigLexer.cpp
│   │   ├── ConfigParser.cpp
│   │   └── ServerConfig.cpp
│   ├── cgi
│   │   └── CGIHandler.cpp
│   ├── main.cpp
│   ├── server
│   │   ├── Response.cpp
│   │   ├── Server.cpp
│   │   ├── ServerSocket.cpp
│   │   └── SocketManager.cpp
│   ├── todo.txt
│   └── utils
│       ├── FileUploadHandler.cpp
│       ├── MultipartParser.cpp
│       ├── file_utils.cpp
│       ├── request_parser.cpp
│       └── utils.cpp
├── tester
└── www
    ├── 404.html
    ├── docs
    │   └── doc_index.html
    ├── images
    │   ├── img.html
    │   ├── index.html
    │   └── test.txt
    ├── index.html
    └── test.html

```

`webpost/Dev_A`:

```
👨‍💻 Developer A: Low-Level Network & Core Server Architecture
1. Parse Configuration File (basic setup)

    Implement logic to read and validate the configuration file path.

    Parse core values like server blocks, ports, and basic error page paths.

    Make this accessible to the rest of the application.

2. Create Non-blocking Sockets and Bind to Ports

    Use socket(), bind(), listen() for TCP socket setup.

    Use fcntl() or similar to set sockets to non-blocking mode.

    Bind to multiple ports as defined in the config.

3. Implement poll/select/kqueue/epoll Loop

    Implement one I/O multiplexing method (poll recommended for portability).

    Centralize event handling (incoming connections, data to read, buffers to flush).

    Ensure the loop handles thousands of connections efficiently.

4. Accept and Manage Client Connections

    Accept new clients without blocking (accept() within the event loop).

    Store client connection metadata (IP, socket fd, timestamps, etc.).

    Maintain a per-client state (e.g. receiving headers, body, etc.).

5. Handle Disconnects and Socket I/O

    Detect when clients disconnect gracefully or unexpectedly.

    Free resources and remove their descriptors from poll().

6. Parse HTTP Requests (GET, POST, DELETE)

    Build an HTTP parser (state machine) to:

        Extract the request line, headers, and body.

        Handle chunked transfer-encoding.

        Detect malformed or oversized requests.

7. Support Chunked Body Parsing

    Reassemble chunked transfer-encoded bodies.

    Ensure bodies are unchunked before further processing (e.g., CGI).

8. Format HTTP Responses

    Compose response strings based on request outcome:

        Correct status codes (200 OK, 404 Not Found, etc.)

        Headers (Content-Length, Content-Type, etc.)

        Body content (HTML, static file, or CGI output)

9. Implement Default Error Pages

    Define and serve generic HTML templates for errors (e.g., 404, 500).

    Allow overrides via the config file.
```

`webpost/Dev_B`:

```
🧠 Developer B: High-Level Logic & Features
1. Design and Parse Configuration File Options

    Extend the config parser to support:

        Server blocks with multiple settings

        Routes with directives (allowed methods, root, autoindex, etc.)

        CGI execution settings

2. Implement Route Matching and Static File Serving

    Map URLs to file system paths based on route blocks.

    Return appropriate status if file not found or method not allowed.

    Resolve relative paths securely.

3. Serve Index Files for Directories

    If the request is for a directory, serve the default index file (like index.html) if it exists.

4. Implement Directory Listing Feature

    If autoindex is enabled in a route, generate and serve an HTML page with file links.

5. Execute CGI Scripts via fork/execve

    Detect CGI extensions (e.g., .php) and:

        Fork a child process

        Set up stdin, stdout via pipes

        Execute CGI with the proper environment and arguments

    Capture and parse the CGI response.

6. Unchunk Body for CGI Input, Handle CGI Output

    Convert chunked request body to plain before passing to CGI.

    Read CGI response (possibly chunked or unbounded) and forward to client.

7. Handle POST File Uploads

    Parse multipart/form-data or raw POST body.

    Store uploaded file to the destination specified in config.

    Enforce upload limits per route.

8. Enforce Max Body Size Per Route

    Reject requests that exceed allowed body size with a 413 Payload Too Large.

9. Add Redirect and Allowed Method Rules

    Respond with a 3xx redirect if defined in config.

    Respond with 405 Method Not Allowed if the method isn't permitted.
```

`webpost/Functions`:

```
🧵 Process and Execution

    execve – Executes a program, replacing the current process image.

    fork – Creates a new process (a child) that is a copy of the current process.

    waitpid – Waits for a child process to change state (e.g., exit).

    kill – Sends a signal to a process (commonly used to terminate it).

    signal – Sets up a handler for specific signals (like SIGINT).

    strerror – Returns a human-readable string describing an errno value.

    errno – A global variable set by system calls when an error occurs.

🧠 Memory & Error Info

    gai_strerror – Returns a string describing a getaddrinfo error.

🛠️ File Descriptors

    dup, dup2 – Duplicate file descriptors; dup2 lets you specify the new FD.

    fcntl – Performs operations on file descriptors (e.g., non-blocking mode).

🧭 Networking (Sockets)

    socket – Creates a socket.

    bind – Binds a socket to an address and port.

    listen – Marks the socket as passive (ready to accept connections).

    accept – Accepts a new incoming connection on a listening socket.

    connect – Initiates a connection on a socket.

    send, recv – Sends or receives data over a socket.

    setsockopt – Sets options on sockets (e.g., reuse address).

    getsockname – Gets the address bound to a socket.

    getprotobyname – Gets protocol info by name (e.g., "tcp").

📡 Addressing & Name Resolution

    htons, htonl – Convert values from host to network byte order (short and long).

    ntohs, ntohl – Convert values from network to host byte order.

    getaddrinfo – Resolves hostnames and service names to address structures.

    freeaddrinfo – Frees memory allocated by getaddrinfo.

⏳ Multiplexing (I/O Waiting)

    select – Monitors multiple file descriptors to see if I/O is possible.

    poll – Similar to select but more scalable.

    epoll_create, epoll_ctl, epoll_wait – Linux-specific scalable I/O event notification system.

    kqueue, kevent – BSD/macOS equivalent of epoll for I/O event notification.

📁 Filesystem Operations

    access – Checks if the program can access a file (read/write/exec).

    stat – Retrieves information about a file (e.g., size, permissions).

    open, read, write, close – Basic file operations.

    opendir, readdir, closedir – Work with directories (open, read entries, close).

    chdir – Changes the current working directory.

```

`webpost/Makefile`:

```
NAME = ./webserv
CXX = c++

INCDIRS = ./includes
CXXFLAGS = -g -Wall -Wextra -Werror -std=c++98 -I$(INCDIRS)

SRCS = \
	./srcs/main.cpp \
	./srcs/cfg/Config.cpp \
	./srcs/cfg/ConfigLexer.cpp \
	./srcs/cfg/ConfigParser.cpp \
	./srcs/server/ServerSocket.cpp \
	./srcs/server/SocketManager.cpp \
	./srcs/server/Response.cpp \
	./srcs/utils/file_utils.cpp \
	./srcs/utils/request_parser.cpp \
	./srcs/utils/utils.cpp

OBJS = $(SRCS:.cpp=.o)

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
```

`webpost/TODOmax`:

```
✅ Your pending tasks as Dev A:
Feature	Status	What you need to do next
config.conf parsed	✅ Done	Already implemented and parsed into your config objects.
ServerConfig populated	✅ Done	You have server configuration objects filled properly.
Call addServer() when parsing config	⚠️ Unclear	Double-check if your parsed ServerConfig objects are effectively registered into your internal runtime server list. Implement if missing.
Map socket FD to ServerConfig at runtime	❌ Not done	When you bind and listen on a socket, you must create a map between that socket's FD and the corresponding ServerConfig. This allows routing incoming connections to the right config block.
Use ServerConfig for runtime behaviors (root, index, error_page)	❌ Not done	Ensure that your HTTP handlers use the config from the matching ServerConfig to locate the root, serve the correct index file, and show the configured error page.
test


https://beej.us/guide/bgnet/
```

`webpost/TODOupdated`:

```
✅ COMPLETED
🧠 1. Non-blocking Server Loop (poll() + socket setup)

What it is:
You’ve built a server that uses poll() to handle multiple connections without blocking. This means the server can talk to many clients at the same time, even if they’re slow or not responding.

Why it's required:
The subject explicitly says:

"Your server must remain non-blocking at all times."

If your server used blocking recv() or accept(), it could freeze waiting for a single client. Using poll() lets you avoid that.

📁 2. Config Parsing (config.conf)

What it is:
Your server reads settings from a config file — just like NGINX or Apache. You support:

listen (port)

server_name

location blocks

root, index, allowed_methods, error_page, upload_path, etc.

Why it's required:
The subject wants your server to be configurable without recompiling. It mimics a real-world HTTP server where admin configures routing, behavior, and limits using a file.

📄 3. Serving Static Files (GET)

What it is:
A request like GET /index.html returns the contents of ./www/index.html.

Why it's required:
A core part of an HTTP server. Users expect the ability to browse and retrieve files. Without this, it’s not a web server.

🚫 4. Custom Error Pages (403, 404)

What it is:
If a file is missing (404) or forbidden (403), your server checks if a custom error page like /404.html is configured and uses it.

Why it's required:
Customizability is key in real servers. Admins don’t want ugly default messages — they want control over how errors look.

⚙️ 5. HTTP Response Builder

What it is:
You generate raw HTTP responses with proper headers (Content-Type, Content-Length, etc).

Why it's required:
Clients expect strict adherence to the HTTP/1.1 protocol. Without correct headers, browsers or curl may behave unpredictably.

🟡 IN PROGRESS / PARTIAL
🗂️ 6. Routing Logic (Matching Location Blocks)

What it is:
When a user accesses /upload, it should follow settings inside:
	location /upload {
  ...
}
Right now, you're only partially using location settings.

Why it's required:
This is the core mechanism to define different behavior per path (e.g., CGI for /cgi-bin/, uploads for /upload, etc).
Without this, your server behaves the same no matter what’s requested.

⬆️ 7. Upload Handling (POST)

What it is:
When a browser sends a form with a file (method POST), your server should read the body, parse it (usually multipart/form-data), and save it in the location set by upload_path.

Why it's required:
Uploading is part of HTTP/1.1 and web forms. The subject asks you to support file uploads, so users can POST files via HTML.

🧠 8. CGI Support (in progress in repo)

What it is:
CGI is a way to execute scripts on the server (e.g., PHP, Python) and return the result as the HTTP response.

You already have a CGIHandler.cpp file. What’s missing is connecting it to requests that match location with a cgi_extension.

Why it's required:
The subject requires CGI support — this is one of the core challenges. It proves you can:

spawn processes

handle pipes

manage environment variables

work with I/O redirection (dup2)

✅ 9. allowed_methods (but not enforced yet)

What it is:
You parse the allowed_methods GET POST line in config.conf — but don’t yet enforce it. If someone sends DELETE and it’s not allowed, your server should return 405.

Why it's required:
The subject wants you to respect and enforce allowed methods for each location.

❌ NOT YET IMPLEMENTED
📂 10. Autoindex

What it is:
If someone accesses a directory like /files/ and no index.html exists, but autoindex on; is set — your server should return a dynamically-generated HTML list of files in that directory.

Why it's required:
This is a required feature per subject and mimics autoindex in NGINX.

📬 11. Full Body Parsing (for POST, CGI, uploads)

What it is:
Your current handleClientRead doesn’t wait for body content (e.g., in a POST). It just reads until \r\n\r\n.

Why it's required:
To upload files or run CGI scripts that read from stdin, you must:

wait until Content-Length bytes are received

pass the body properly

❌ 12. Method Enforcement

What it is:
If a method is not listed in allowed_methods, return 405 Method Not Allowed.

Why it's required:
The subject says:

You must handle 405 Method Not Allowed if method is disallowed by location config.

⚙️ 13. CGI Integration (missing)

What it is:
The code exists but is not plugged into the request handler. You must:

detect when a location matches .php or .py

fork + dup2

wait on child

use response

Why it's required:
Mandatory for full marks. Also one of the most complex parts (pipes, env, fork).

🌐 14. HTTP/1.1 Features (Keep-Alive, etc.)

What it is:
Your server closes the connection after every response. That’s fine, but HTTP/1.1 allows persistent connections. It’s optional but nice.

Why it's optional:
Subject doesn’t require full spec — just core behavior. But this is a bonus-level polish.

🎯 Overall Summary
Category	Features	Status
Core Socket Server	Non-blocking, poll(), accept(), read/write	✅ Done
Config System	Fully working parser with location support	✅ Done
Static File Handling	200, 403, 404, custom error pages	✅ Done
HTTP Parser	Simplified GET/POST (no full body)	🟡 Partial
Routing Logic	Location-based settings partially respected	🟡 Partial
Upload & POST	Upload logic present, not wired in	🟡 Partial
CGI Support	Code exists, not plugged in	🟡 Partial
Method Checks	Parsed but not enforced	❌ Missing
Autoindex	Directory listing not implemented	❌ Missing
Bonus / Polishing	Logging, Keep-Alive, HTTPS, etc.	❌ Optional
```

`webpost/Webserv_Project_Task_Checklist.csv`:

```csv
Task,Assigned To,Done
Parse configuration file (basic setup),Developer A (Low-Level Network & Core),False
Create non-blocking sockets and bind to ports,Developer A (Low-Level Network & Core),False
Implement poll/select/kqueue/epoll loop,Developer A (Low-Level Network & Core),False
Accept and manage client connections,Developer A (Low-Level Network & Core),False
Handle disconnects and socket I/O,Developer A (Low-Level Network & Core),False
"Parse HTTP requests (GET, POST, DELETE)",Developer A (Low-Level Network & Core),False
Support chunked body parsing,Developer A (Low-Level Network & Core),False
Format HTTP responses with correct status codes,Developer A (Low-Level Network & Core),False
Implement default error pages,Developer A (Low-Level Network & Core),False
"Design and parse configuration file options (routing, error pages)",Developer B (High-Level Logic & Features),False
Implement route matching and static file serving,Developer B (High-Level Logic & Features),False
Serve index files for directories,Developer B (High-Level Logic & Features),False
Implement directory listing feature,Developer B (High-Level Logic & Features),False
Execute CGI scripts via fork/execve,Developer B (High-Level Logic & Features),False
"Unchunk body for CGI input, handle CGI output",Developer B (High-Level Logic & Features),False
Handle POST file uploads to proper location,Developer B (High-Level Logic & Features),False
Enforce max body size per route,Developer B (High-Level Logic & Features),False
Add redirect and allowed method rules,Developer B (High-Level Logic & Features),False
"Write unit and integration tests (e.g., in Python)",Joint Tasks,False
Create test configuration files and websites,Joint Tasks,False
Write Makefile and ensure build rules work,Joint Tasks,False
"Write documentation (README, config usage)",Joint Tasks,False
Perform stress tests and benchmark,Joint Tasks,False
Final integration and debugging,Joint Tasks,False

```

`webpost/config.conf`:

```conf
server {
    listen        8081;
    server_name   localhost;
    root          ./www;
    error_page    404 /404.html;

    location / {
        root           ./www/html;
        index          index.html;
        allowed_methods  GET POST;
        autoindex      off;
    }

    location /upload {
        root           ./www/html;
        allowed_methods  POST;
        upload_path    ./www/uploads;
        max_body_size  1048576;
    }

    location /cgi-bin/ {
        root           ./www/html;
        cgi_extension  .php;
        cgi_path       /usr/bin/php-cgi;
    }

    location /old-path {
        redirect       /new-path;
    }
}

```

`webpost/configlocationtest.conf`:

```conf
server {
    listen 8081;
    server_name localhost;
    root ./www;
    index index.html;

    location / {
        root ./www;
        index index.html;
    }

    location /images/ {
        root ./www/images;
        index img.html;
    }

    location /docs {
        root ./www/docs;
        index doc_index.html;
    }
}
```

`webpost/cursus`:

```
https://limewire.com/d/Ea5zE#CFvpHhMUoM
```

`webpost/includes/CGIHandler.hpp`:

```hpp
#ifndef CGI_HANDLER_HPP
#define CGI_HANDLER_HPP

#include <string>
#include <vector>

// CGIHandler provides a method to execute a CGI script via fork/execve.
// It sets up pipes to pass the request body to the CGI process and to capture its output.
class CGIHandler
{
    public:
        // Executes a CGI script.
        // - cgiPath: Full path to the CGI executable (e.g., /usr/bin/php-cgi)
        // - args: Vector of arguments for the CGI script.
        //         Note: args[0] is typically the script itself.
        // - env: Vector of environment variables (as "KEY=VALUE" strings).
        // - input: The full request body (after unchunking, if needed).
        // Returns the output produced by the CGI script.
        static std::string execute(const std::string &cgiPath,
                                   const std::vector<std::string> &args,
                                   const std::vector<std::string> &env,
                                   const std::string &input);
};

#endif

```

`webpost/includes/Config.hpp`:

```hpp
# ifndef CONFIG_HPP
# define CONFIG_HPP

# include <string>
# include <vector>
# include <set>
# include <map>

struct RouteConfig
{
    std::string path;
    std::set<std::string> allowed_methods;
    bool        autoindex;
    size_t      max_body_size;
    std::string root;
    std::string index;
    std::string upload_path;
    std::string redirect;
    std::string cgi_extension;
    std::string cgi_path;
    
    RouteConfig();
};

struct ServerConfig
{
    std::string host;
    int         port;
    std::string server_name;
    std::string root;
    std::string index;
    std::map<int, std::string> error_pages;
    std::vector<RouteConfig> routes;
    size_t client_max_body_size;

    ServerConfig();
};

struct Config
{
    std::vector<ServerConfig> servers;
};

#endif
```

`webpost/includes/ConfigLexer.hpp`:

```hpp
#ifndef CONFIG_LEXER_HPP
#define CONFIG_LEXER_HPP

#include <string>
#include <vector>

enum TokenType
{
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_KEYWORD,
    TOKEN_SYMBOL,    // chars bizarre '{', '}', ';'
    TOKEN_END_OF_FILE
};

struct Token
{
    TokenType type;
    std::string value;
    int line; // pour report errors
};

class ConfigLexer
{
public:
    ConfigLexer(const std::string& filename);
    const std::vector<Token>& tokenize();
    
private:
    std::string readFile(const std::string& filename);
    void lex();
    
    std::string _source;
    std::vector<Token> _tokens;
    size_t _current;
    int _line;
};

#endif

```

`webpost/includes/ConfigParser.hpp`:

```hpp
#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

//#include "ServerConfig.hpp"
#include "Config.hpp"
#include "ConfigLexer.hpp" 
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>

class ConfigParser
{
	private:
	std::string m_filePath;
	std::vector<ServerConfig> m_servers;

	void parse();                    // Parses entire file
	//void parseServer(std::istream&); // Parses a single `server` block

	//static std::string trim(const std::string& line);
	//static std::vector<std::string> split(const std::string& line);
	// Recursively parses a server block from tokens.
	ServerConfig parseServerBlock(const std::vector<Token>& tokens, size_t &current);
	RouteConfig parseLocationBlock(const std::vector <Token>& tokens, size_t &current);

	public:
	ConfigParser();
	ConfigParser(const std::string &path);
	ConfigParser(const ConfigParser &src);
	ConfigParser &operator=(const ConfigParser &src);
	~ConfigParser();

	const std::vector<ServerConfig> &getServers() const;
};

	//some printer for debug
void printRouteConfig(const RouteConfig &route);
void printServerConfig(const ServerConfig &server);

#endif

```

`webpost/includes/FileUploadHandler.hpp`:

```hpp
#ifndef FILEUPLOADHANDLER_HPP
#define FILEUPLOADHANDLER_HPP

#include <string>
#include "MultipartParser.hpp"

class FileUploadHandler
{
public:
    // Saves the uploaded file part to the specified directory.
    // Optionally enforces a maximum file size (0 means no limit).
    // Returns the full path where the file was saved.
    static std::string saveUploadedFile(const MultipartPart &part,
                                          const std::string &uploadPath,
                                          size_t maxFileSize = 0);
};

#endif

```

`webpost/includes/MultipartParser.hpp`:

```hpp
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   MultipartParser.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: wkeiser <wkeiser@student.42mulhouse.fr>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 21:04:40 by wkeiser           #+#    #+#             */
/*   Updated: 2025/04/13 21:04:41 by wkeiser          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MULTIPARTPARSER_HPP
#define MULTIPARTPARSER_HPP

#include <string>
#include <vector>
#include <map>

// Structure for storing a parsed part (form field or file upload)
struct MultipartPart
{
    std::map<std::string, std::string> headers;
    std::string content;
    bool isFile;         // true if this part represents a file upload
    std::string filename; // filename if provided in headers
};

class MultipartParser
{
public:
    // Constructor takes the full request body and the Content-Type header.
    MultipartParser(const std::string &body, const std::string &contentType);
    ~MultipartParser();

    // Parse returns a vector of MultipartPart entries.
    std::vector<MultipartPart> parse();

private:
    // Helper to extract the boundary string from the content type.
    std::string extractBoundary(const std::string &contentType) const;

    std::string _body;
    std::string _contentType;
};

#endif

```

`webpost/includes/Server.hpp`:

```hpp
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/01 12:47:21 by mgovinda          #+#    #+#             */
/*   Updated: 2025/05/24 19:58:29 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include "ServerConfig.hpp"
#include "SocketManager.hpp"
#include "request_response_struct.hpp" // for Response

// Forward declare parser
#include "RequestParser.hpp"

class Server
{
	private:
    ServerConfig   _config;
    SocketManager  _sockMgr;

	public:
	Server();
	Server(const Server &src);
	explicit Server(const ServerConfig& config);
	Server &operator=(const Server &src);
	~Server();

	 // Start accepting connections & dispatch requests
	 void run();
};

#endif
```

`webpost/includes/ServerSocket.hpp`:

```hpp
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerSocket.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/08 16:35:57 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/16 19:19:41 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERSOCKET_HPP
#define SERVERSOCKET_HPP

#include <string>

class ServerSocket
{
	private:
	int				m_fd;
	unsigned short	m_port;
	std::string		m_host;

	
	void setNonBlocking();
	void bindSocket();
	void listenSocket();
	ServerSocket &operator=(const ServerSocket &src);
	ServerSocket(const ServerSocket &src);
	
	public:
	ServerSocket();
	ServerSocket(const std::string &host, unsigned short port);
	~ServerSocket();

	//get
	int	getFd() const;
	unsigned short	getPort() const;
	const std::string &getHost() const;

	//set
	void setup(); //does socket(), bind(), listen();
	bool isValid() const;
};

#endif
```

`webpost/includes/SocketManager.hpp`:

```hpp
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:22 by mgovinda          #+#    #+#             */
/*   Updated: 2025/08/18 18:02:31 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SOCKETMANAGER_HPP
#define SOCKETMANAGER_HPP

#include "ServerSocket.hpp"
#include "utils.hpp"
#include "Config.hpp"
#include "request_parser.hpp"
#include <vector>
#include <poll.h>
#include <map>
#include <string>
#include <set> // NEW

class SocketManager
{
	private:
	std::vector<ServerSocket*> m_servers;
	std::vector<struct pollfd> m_pollfds;
	std::set<int>                  m_serverFds;     // to distinguish server vs client
    std::map<int, std::string>     m_clientBuffers; // client fd → partial data
	std::map<int, std::string>	m_clientWriteBuffers;
	std::vector<ServerConfig> m_serversConfig;
	Config	m_config;

	std::map<int, size_t> m_clientToServerIndex;

	SocketManager &operator=(const SocketManager &src);
	SocketManager(const SocketManager &src);

	public:
	SocketManager(const Config &config);
	~SocketManager();
	SocketManager();

	void addServer(const std::string& host, unsigned short port);
	void initPoll();
	void run(); // infinite poll loop

	// member func inside run()
	bool isListeningSocket(int fd) const;
	void handleNewConnection(int listen_fd);
	void handleClientRead(int fd);
	void handleClientDisconnect(int fd);
	void handleClientWrite(int fd);
	std::string buildErrorResponse(int code, const ServerConfig &server);
	const ServerConfig& findServerForClient(int fd) const;
	void setPollToWrite(int fd);

	void setServers(const std::vector<ServerConfig> & servers);
};

#endif
```

`webpost/includes/file_utils.hpp`:

```hpp
#ifndef FILE_UTILS_HPP
#define FILE_UTILS_HPP
#include <string>

bool fileExists(const std::string& path);
std::string readFile(const std::string& path);
bool isPathSafe(const std::string& base, const std::string& target);
bool isPathTraversalSafe(const std::string &path);
 namespace file_utils {
       // Does this path exist?
       bool exists(const std::string& path);
    
       // Read the entire file into a string
       std::string readFile(const std::string& path);
    
       // Simplify “/foo/../bar” etc. into a canonical path
       std::string normalize(const std::string& rawPath);
    
       // Guess the MIME type (e.g. ".html" → "text/html")
       std::string getMimeType(const std::string& path);
     }

#endif

```

`webpost/includes/request_parser.hpp`:

```hpp
#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

#include "request_reponse_struct.hpp"
#include <string>

Request parseRequest(const std::string &raw);

#endif

```

`webpost/includes/request_reponse_struct.hpp`:

```hpp
#ifndef REQUEST_REPONSE_STRUCT_HPP
#define REQUEST_REPONSE_STRUCT_HPP

#include <map>
#include <string>

struct Request {
    std::string method;
    std::string path;
    std::string http_version;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct Response {
    int status_code;                      // e.g., 200, 404, 500
    std::string status_message;           // e.g., "OK", "Not Found", "Internal Server Error"
    std::map<std::string, std::string> headers;  // e.g., Content-Type, Content-Length
    std::string body;                     // The actual body content (HTML, file data, CGI output)
    bool close_connection;                // Whether to close connection after response (Connection: close)
};

std::string build_http_response(const Response &res);

#endif
```

`webpost/includes/to_delete_ServerConfig.hpp`:

```hpp
#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

/*ALMOST THE SAME AS CONFIG.HPP NEED TO THIS ONE AND USE CONFIG.HPP*/


#include <string>
#include <map>
#include <vector>

struct LocationConfig
{
	std::string path; // "/images"
	std::string root;                         // "/var/www/images"
    std::string index;                        // "index.html"
    std::vector<std::string> allowedMethods;  // {"GET", "POST"}
    bool autoindex;                           // true / false
    std::string redirect;                     // "301 http://example.com" (optional)
    std::string uploadPath;                   // "/tmp/uploads"
    std::string cgiPath;                      // "/usr/bin/php-cgi"
    std::string cgiExtension;                 // ".php"
	
	LocationConfig() : autoindex(false) { }
};

class ServerConfig
{
	private:
	std::string					m_host; //"127.0.0.1"
	int							m_port; //8080
	std::string					m_servername; //optional
	std::map<int, std::string>	m_error_pages;
	std::vector<LocationConfig>	m_locations;
	size_t 						m_clientMaxBodySize;

	public:

	//Form canonical
	ServerConfig();
	ServerConfig(const ServerConfig &src);
	ServerConfig &operator=(const ServerConfig &src);
	~ServerConfig();

	//Get
	const std::string& getHost() const;
	int getPort() const;
	const std::string& getServerName() const;
	const std::map<int, std::string>& getErrorPages() const;
	size_t getClientMaxBodySize() const;
	const std::vector<LocationConfig>& getLocations() const;

	//Set
	void setHost(const std::string& host);
	void setPort(int port);
	void setServerName(const std::string& name);
	void addErrorPage(int code, const std::string& path);
	void setClientMaxBodySize(size_t size);
	void addLocation(const LocationConfig& loc);
	const LocationConfig* findBestLocation(const std::string& requestPath) const;
};

#endif

```

`webpost/includes/utils.hpp`:

```hpp
#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>
#include "Config.hpp"

std::string to_string(size_t val);
const RouteConfig *findMatchingLocation(const ServerConfig &server, const std::string &path);

#endif
```

`webpost/srcs/cfg/Config.cpp`:

```cpp
#include "Config.hpp"

ServerConfig::ServerConfig () :
	host("0.0.0.0"), port(80), client_max_body_size(1000000)
{
	return ;
}

RouteConfig::RouteConfig() :
	autoindex(false),
	max_body_size(0)
{
	return ;
}
```

`webpost/srcs/cfg/ConfigLexer.cpp`:

```cpp
#include "ConfigLexer.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <iostream>

ConfigLexer::ConfigLexer(const std::string& filename)
    : _current(0), _line(1)
{
    _source = readFile(filename);
    lex();
}

std::string ConfigLexer::readFile(const std::string& filename)
{
    std::ifstream file(filename.c_str());
    if (!file) {
        throw std::runtime_error("Unable to open configuration file: " + filename);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

const std::vector<Token>& ConfigLexer::tokenize()
{
    return _tokens;
}

void ConfigLexer::lex()
{
    while (_current < _source.length())
    {
        char c = _source[_current];

        if (std::isspace(c))
        {
            if (c == '\n') _line++;
            _current++;
            continue;
        }

        // SYMBOLS: { } ;
        if (c == '{' || c == '}' || c == ';')
        {
            Token token;
            token.type = TOKEN_SYMBOL;
            token.value = c;
            token.line = _line;
            _tokens.push_back(token);
            _current++;
            continue;
        }

        // NUMBERS
        if (std::isdigit(c))
        {
            std::string number;
            while (_current < _source.length() && std::isdigit(_source[_current]))
            {
                number.push_back(_source[_current]);
                _current++;
            }
            Token token;
            token.type = TOKEN_NUMBER;
            token.value = number;
            token.line = _line;
            _tokens.push_back(token);
            continue;
        }

        // IDENTIFIERS or PATHS (stop before semicolon if needed)
        if (std::isalpha(c) || c == '_' || c == '/' || c == '.')
        {
            std::string identifier;
            while (_current < _source.length() &&
                   (std::isalnum(_source[_current]) ||
                    _source[_current] == '_' ||
                    _source[_current] == '-' ||
                    _source[_current] == '/' ||
                    _source[_current] == '.' ||
                    _source[_current] == ':'))
            {
                if (_source[_current] == ';') // stop at semicolon
                    break;
                identifier.push_back(_source[_current]);
                _current++;
            }

            Token token;
            token.type = TOKEN_IDENTIFIER;
            token.value = identifier;
            token.line = _line;
            _tokens.push_back(token);

            // If next char is a semicolon, push it as a separate token
            if (_current < _source.length() && _source[_current] == ';') {
                Token semi;
                semi.type = TOKEN_SYMBOL;
                semi.value = ";";
                semi.line = _line;
                _tokens.push_back(semi);
                _current++;
            }

            continue;
        }

        // Skip unknown characters (optional: you can throw if you want strict syntax)
        _current++;
    }

    Token eof;
    eof.type = TOKEN_END_OF_FILE;
    eof.value = "";
    eof.line = _line;
    _tokens.push_back(eof);
}

```

`webpost/srcs/cfg/ConfigParser.cpp`:

```cpp
#include "ConfigParser.hpp"
#include "ConfigLexer.hpp"
#include "Config.hpp"
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <sstream>

/*TODO change from the Serverconfig class (.hpp) to the struct from Config.hpp
    JAI FAIS UN CHANGEMENT POUR errorpage 404 qui etait std::stirng et mtn cest un std::map<int, std::string> error_pages
    si ya des bug la dessus cest surement que jai pas gerer le refactoring
*/

ConfigParser::ConfigParser() : m_filePath("")
{
    parse();
}

ConfigParser::ConfigParser(const std::string &path) : m_filePath(path)
{
    this->parse();
}

ConfigParser::ConfigParser(const ConfigParser &src)
{
    *this = src;
}

ConfigParser &ConfigParser::operator=(const ConfigParser &src)
{
    if (this != &src)
    {
        this->m_filePath = src.m_filePath;
        this->m_servers = src.m_servers;
    }
    return (*this);
}

ConfigParser::~ConfigParser() {}

const std::vector<ServerConfig> &ConfigParser::getServers() const
{
    return m_servers;
}

void ConfigParser::parse()
{
    std::ifstream file(this->m_filePath.c_str());
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open configuration file.");
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string configContent = buffer.str();
    file.close();

    ConfigLexer lexer(this->m_filePath);
    std::vector<Token> tokens = lexer.tokenize();

    size_t current = 0;
    while (current < tokens.size())
    {
        if (tokens[current].value == "server")
        {
            current++;
            ServerConfig serverConfig = parseServerBlock(tokens, current);
            this->m_servers.push_back(serverConfig);
        }
        else
        {
            current++;
        }
    }
}

// Modified parseServerBlock() method
/* ServerConfig ConfigParser::parseServerBlock(const std::vector<Token>& tokens, size_t &current)
{
    ServerConfig server;
    if (tokens[current].value != "{")
    {
        throw std::runtime_error("Expected '{' at beginning of server block.");
    }
    current++;

    while (current < tokens.size() && tokens[current].value != "}")
    {
        if (tokens[current].value == "host")
        {
            current++;
            if (current < tokens.size())
            {
                server.host = listenValue.substr(0, colon);
                server.port = std::atoi(listenValue.c_str() + colon + 1);
            }
            else
            {
                server.port = std::atoi(listenValue.c_str());
            }
            if (current >= tokens.size() || tokens[current].value != ";")
            {
                std::ostringstream oss;
                oss << "Attendu ';' après la directive 'listen' à la ligne " << tokens[current].line;
                throw std::runtime_error(oss.str());
            }
            current++;  // Consomme ';'
        }
        else if (directive == "server_name")
        {
            if (current >= tokens.size())
            {
                throw std::runtime_error("Fin inattendue des jetons dans la directive 'server_name'");
            }
            server.server_name = tokens[current].value;
            current++;
            if (current >= tokens.size() || tokens[current].value != ";")
            {
                throw std::runtime_error("Attendu ';' après la directive 'server_name'");
            }
            current++;  // Consomme ';'
        }
        else if (directive == "error_page")
        {
            if (current + 1 >= tokens.size())
            {
                throw std::runtime_error("Fin inattendue des jetons dans la directive 'error_page'");
            }
            int code = std::atoi(tokens[current].value.c_str());
            current++;
            std::string path = tokens[current].value;
            current++;
            if (current >= tokens.size() || tokens[current].value != ";")
            {
                std::ostringstream oss;
                oss << "Attendu ';' après la directive 'error_page' à la ligne " << tokens[current - 1].line;
                throw std::runtime_error(oss.str());
            }
            server.error_pages[code] = path;
            current++;  // consume ';'
        }
        else if (directive == "client_max_body_size")
        {
            if (current >= tokens.size())
            {
                throw std::runtime_error("Fin inattendue des jetons dans la directive 'client_max_body_size'");
            }
            server.client_max_body_size = std::atoi(tokens[current].value.c_str());
            current++;
            if (current >= tokens.size() || tokens[current].value != ";")
            {
                throw std::runtime_error("Attendu ';' après la directive 'client_max_body_size'");
            }
            current++;  // Consomme ';'
        }
        // here is just added the parse Location bloc that call parseLocationBlock()
        else if (directive == "location")
        {
            if (current >= tokens.size())
                throw std::runtime_error("Expected path after 'location'");
            RouteConfig route;
            route.path = tokens[current++].value;

            if (current >= tokens.size() || tokens[current++].value != "{")
                throw std::runtime_error("Expected '{' after location path");

            route = parseLocationBlock(tokens, current);
            server.routes.push_back(route);
        }
        else if (directive == "root") 
        {
            if (current >= tokens.size())
                throw std::runtime_error("Unexpected end of tokens in 'root'");
        server.root = tokens[current++].value;
        if (tokens[current++].value != ";")
        throw std::runtime_error("Expected ';' after root");
        }
        else if (directive == "index")
        {
            if (current >= tokens.size())
                throw std::runtime_error("Unexpected end of tokens in 'index'");
            server.index = tokens[current++].value;
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after index");
        }
        else 
        {
            std::cerr << "Unknown directive in location: " << directive << std::endl;

            // Skip until ';' or '}'
            while (current < tokens.size() && 
                tokens[current].value != ";" && 
                tokens[current].value != "}")
            {
                current++;
            }
            if (current < tokens.size() && tokens[current].value == ";")
                current++; // skip semicolon
        }
        else if (tokens[current].value == "server_name")
        {
            current++;
            if (current < tokens.size())
            {
                server.setServerName(tokens[current].value);
                current++;
            }
            if (tokens[current].value == ";")
            {
                current++;
            }
        }
        else if (tokens[current].value == "client_max_body_size")
        {
            current++;
            if (current < tokens.size())
            {
                server.setClientMaxBodySize(static_cast<size_t>(strtoul(tokens[current].value.c_str(), NULL, 10)));
                current++;
            }
            if (tokens[current].value == ";")
            {
                current++;
            }
        }
        else if (tokens[current].value == "location")
        {
            current++;
            // NEW: Parse location block and add as a route configuration
            LocationConfig location = parseLocationBlock(tokens, current);
            server.addLocation(location);
        }
        else
        {
            current++;
        }
    }
    if (current < tokens.size() && tokens[current].value == "}")
    {
        current++;
    }
    current++;  // Consomme '}'
    return (server);
} */

// Parse fixed for build response error test:

ServerConfig ConfigParser::parseServerBlock(const std::vector<Token>& tokens, size_t &current)
{
	ServerConfig server;

	if (tokens[current].value != "{")
		throw std::runtime_error("Expected '{' at beginning of server block.");
	current++;

	while (current < tokens.size() && tokens[current].value != "}")
	{
		std::string directive = tokens[current].value;
		current++;

		if (directive == "listen")
		{
			if (current >= tokens.size()) throw std::runtime_error("Missing value for 'listen'");
			std::string listenValue = tokens[current++].value;

			size_t colon = listenValue.find(':');
			if (colon != std::string::npos)
			{
				server.host = listenValue.substr(0, colon);
				server.port = std::atoi(listenValue.c_str() + colon + 1);
			}
			else
			{
				server.port = std::atoi(listenValue.c_str());
			}

			if (tokens[current++].value != ";")
				throw std::runtime_error("Expected ';' after 'listen'");
		}
		else if (directive == "server_name")
		{
			if (current >= tokens.size()) throw std::runtime_error("Missing value for 'server_name'");
			server.server_name = tokens[current++].value;
			if (tokens[current++].value != ";")
				throw std::runtime_error("Expected ';' after 'server_name'");
		}
		else if (directive == "root")
		{
			if (current >= tokens.size()) throw std::runtime_error("Missing value for 'root'");
			server.root = tokens[current++].value;
			if (tokens[current++].value != ";")
				throw std::runtime_error("Expected ';' after 'root'");
		}
		else if (directive == "index")
		{
			if (current >= tokens.size()) throw std::runtime_error("Missing value for 'index'");
			server.index = tokens[current++].value;
			if (tokens[current++].value != ";")
				throw std::runtime_error("Expected ';' after 'index'");
		}
		else if (directive == "error_page")
		{
			if (current + 1 >= tokens.size()) throw std::runtime_error("Invalid 'error_page' directive");

			int code = std::atoi(tokens[current++].value.c_str());
			std::string path = tokens[current++].value;

			if (tokens[current++].value != ";")
				throw std::runtime_error("Expected ';' after 'error_page'");

			server.error_pages[code] = path;
		}
		else if (directive == "client_max_body_size")
		{
			if (current >= tokens.size()) throw std::runtime_error("Missing value for 'client_max_body_size'");
			server.client_max_body_size = static_cast<size_t>(strtoul(tokens[current++].value.c_str(), NULL, 10));
			if (tokens[current++].value != ";")
				throw std::runtime_error("Expected ';' after 'client_max_body_size'");
		}

		else if (directive == "location")
        {
            if (current >= tokens.size()) throw std::runtime_error("Expected path after 'location'");
            std::string locPath = tokens[current++].value;

            if (current >= tokens.size() || tokens[current++].value != "{")
                throw std::runtime_error("Expected '{' after location path");

            RouteConfig route = parseLocationBlock(tokens, current);
	        route.path = locPath;  // ✅ set the path after parsing
            std::cout << "[DEBUG] Parsed location path: '" << route.path << "'" << std::endl;
            server.routes.push_back(route);
        }

		else
		{
			std::cerr << "Unknown directive in server block: " << directive << std::endl;

			// Skip to next semicolon or closing brace
			while (current < tokens.size() && tokens[current].value != ";" && tokens[current].value != "}")
				current++;
			if (tokens[current].value == ";")
				current++;
		}
	}

	if (tokens[current].value != "}")
		throw std::runtime_error("Expected '}' to close server block");

	current++; // consume '}'
	return server;
}


RouteConfig ConfigParser::parseLocationBlock(const std::vector<Token>& tokens, size_t &current)
{
    RouteConfig ret;

    while (current < tokens.size() && tokens[current].value != "}") {
        std::string directive = tokens[current++].value;

        if (directive == "allowed_methods") {
            while (tokens[current].value != ";") {
                ret.allowed_methods.insert(tokens[current++].value);
                if (current >= tokens.size())
                    throw std::runtime_error("Unexpected end of allowed_methods");
            }
            current++;
        }
        else if (directive == "root") {
            ret.root = tokens[current++].value;
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after root");
        }
        else if (directive == "index") {
            ret.index = tokens[current++].value;
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after index");
        }
        else if (directive == "upload_path") {
            ret.upload_path = tokens[current++].value;
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after upload_path");
        }
        else if (directive == "autoindex") {
            std::string value = tokens[current++].value;
            ret.autoindex = (value == "on");
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after autoindex");
        }
        else if (directive == "cgi_extension") {
            ret.cgi_extension = tokens[current++].value;
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after cgi_extension");
        }
        else if (directive == "cgi_path") {
            ret.cgi_path = tokens[current++].value;
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after cgi_path");
        }
        else if (directive == "max_body_size") {
            ret.max_body_size = std::atoi(tokens[current++].value.c_str());
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after max_body_size");
        }
        else if (directive == "redirect") {
            ret.redirect = tokens[current++].value;
            if (tokens[current++].value != ";")
                throw std::runtime_error("Expected ';' after redirect");
        }
        else {
            std::cerr << "Unknown directive in location: " << directive << std::endl;
            while (tokens[current].value != ";" && tokens[current].value != "}")
                current++;
            if (tokens[current].value == ";")
                current++;
        }
    }

    if (tokens[current].value != "}")
        throw std::runtime_error("Expected '}' to close location block");
    current++;

    return ret;
}



//some printers for debug

void printRouteConfig(const RouteConfig &route) {
    std::cout << "  Location path: " << route.path << std::endl;
    
    std::cout << "    allowed_methods: ";
    for (std::set<std::string>::const_iterator it = route.allowed_methods.begin();
         it != route.allowed_methods.end(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    std::cout << "    root: " << route.root << std::endl;
    std::cout << "    index: " << route.index << std::endl;
    std::cout << "    upload_path: " << route.upload_path << std::endl;
    std::cout << "    autoindex: " << (route.autoindex ? "on" : "off") << std::endl;
    std::cout << "    cgi_extension: " << route.cgi_extension << std::endl;
    std::cout << "    cgi_path: " << route.cgi_path << std::endl;
    std::cout << "    max_body_size: " << route.max_body_size << std::endl;
    std::cout << "    redirect: " << route.redirect << std::endl;
}

void printServerConfig(const ServerConfig& server) {
    std::cout << "=== Server Config ===" << std::endl;
    std::cout << "host: " << server.host << std::endl;
    std::cout << "port: " << server.port << std::endl;
    std::cout << "server_name: " << server.server_name << std::endl;
    std::cout << "root: " << server.root << std::endl;
    std::cout << "index: " << server.index << std::endl;
    std::cout << "client_max_body_size: " << server.client_max_body_size << std::endl;

    std::cout << "error_pages:" << std::endl;
    for (std::map<int, std::string>::const_iterator it = server.error_pages.begin();
         it != server.error_pages.end(); ++it) {
        std::cout << "  " << it->first << " => " << it->second << std::endl;
    }

    std::cout << "routes:" << std::endl;
    for (size_t i = 0; i < server.routes.size(); ++i) {
        printRouteConfig(server.routes[i]);
    }
}

```

`webpost/srcs/cfg/ServerConfig.cpp`:

```cpp
#include "ServerConfig.hpp"
#include <algorithm>

ServerConfig::ServerConfig()
	: m_host("0.0.0.0"), m_port(80), m_servername(""), m_error_pages(), m_locations(), m_clientMaxBodySize(1000000)
{
	return ;
}

ServerConfig::ServerConfig(const ServerConfig &src)
{
	*this = src;
}

ServerConfig &ServerConfig::operator=(const ServerConfig &src)
{
	if (this != &src) {
		m_host = src.m_host;
		m_port = src.m_port;
		m_servername = src.m_servername;
		m_error_pages = src.m_error_pages;
		m_locations = src.m_locations;
		m_clientMaxBodySize = src.m_clientMaxBodySize;
	}
	return *this;
}

ServerConfig::~ServerConfig()
{
	return ;
}

const std::string& ServerConfig::getHost() const
{
	return m_host;
}

int ServerConfig::getPort() const
{
	return m_port;
}

const std::string& ServerConfig::getServerName() const
{
	return m_servername;
}

const std::map<int, std::string>& ServerConfig::getErrorPages() const
{
	return m_error_pages;
}

size_t ServerConfig::getClientMaxBodySize() const
{
	return m_clientMaxBodySize;
}

const std::vector<LocationConfig>& ServerConfig::getLocations() const
{
	return m_locations;
}


// ===== Setters =====
void ServerConfig::setHost(const std::string &host)
{
	m_host = host;
}

void ServerConfig::setPort(int port)
{
	m_port = port;
}

void ServerConfig::setServerName(const std::string &name)
{
	m_servername = name;
}

void ServerConfig::addErrorPage(int code, const std::string &path)
{
	m_error_pages[code] = path;
}

void ServerConfig::setClientMaxBodySize(size_t size)
{
	m_clientMaxBodySize = size;
}

void ServerConfig::addLocation(const LocationConfig &loc)
{
	m_locations.push_back(loc);
}

const LocationConfig*
ServerConfig::findBestLocation(const std::string& requestPath) const
{
    const LocationConfig*	best  = NULL;
    size_t					bestLen = 0;

    // Try each location block…
    for (std::vector<LocationConfig>::const_iterator it = m_locations.begin(); it != m_locations.end(); ++it)
	{
        const std::string& locPath = it->path;                    // e.g. "/images"
        // does requestPath start with locPath?
        if (requestPath.compare(0, locPath.size(), locPath) == 0)
		{
            // prefer the longest matching prefix
            if (locPath.size() > bestLen)
			{
                bestLen = locPath.size();
                best    = &*it;
            }
        }
    }
    return best;
}
```

`webpost/srcs/cgi/CGIHandler.cpp`:

```cpp
#include "CGIHandler.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <vector>

std::string CGIHandler::execute(const std::string &cgiPath,
                                const std::vector<std::string> &args,
                                const std::vector<std::string> &env,
                                const std::string &input)
{
    // Create pipe for sending input to the CGI process (parent writes, child reads)
    int pipeIn[2];
    if (pipe(pipeIn) < 0)
    {
        throw std::runtime_error("Failed to create input pipe.");
    }
    
    // Create pipe for reading the output from the CGI process (child writes, parent reads)
    int pipeOut[2];
    if (pipe(pipeOut) < 0)
    {
        { // Ensure resources are closed before throwing.
            close(pipeIn[0]);
            close(pipeIn[1]);
        }
        throw std::runtime_error("Failed to create output pipe.");
    }
    
    pid_t pid = fork();
    if (pid < 0)
    {
        { // Clean up pipes on fork failure.
            close(pipeIn[0]);
            close(pipeIn[1]);
            close(pipeOut[0]);
            close(pipeOut[1]);
        }
        throw std::runtime_error("Failed to fork process for CGI.");
    }
    
    if (pid == 0)
    {
        // Child process begins here.
        if (dup2(pipeIn[0], STDIN_FILENO) < 0)
        {
            exit(1);
        }
        if (dup2(pipeOut[1], STDOUT_FILENO) < 0)
        {
            exit(1);
        }
        
        // Close unused pipe ends in the child.
        if (close(pipeIn[0]) < 0) { }
        if (close(pipeIn[1]) < 0) { }
        if (close(pipeOut[0]) < 0) { }
        if (close(pipeOut[1]) < 0) { }
        
        // Prepare argument list for execve.
        std::vector<char*> argv;
        { // Add CGI executable path as first argument.
            argv.push_back(const_cast<char*>(cgiPath.c_str()));
        }
        for (size_t i = 0; i < args.size(); ++i)
        {
            argv.push_back(const_cast<char*>(args[i].c_str()));
        }
        argv.push_back(NULL);
        
        // Prepare environment variables.
        std::vector<char*> envp;
        for (size_t i = 0; i < env.size(); ++i)
        {
            envp.push_back(const_cast<char*>(env[i].c_str()));
        }
        envp.push_back(NULL);
        
        // Execute the CGI script.
        execve(cgiPath.c_str(), argv.data(), envp.data());
        // If execve returns, an error occurred.
        exit(1);
    }
    else
    {
        // Parent process begins here.
        if (close(pipeIn[0]) < 0) { }
        if (close(pipeOut[1]) < 0) { }
        
        // Write the provided input to the child process's STDIN.
        ssize_t write_ret = write(pipeIn[1], input.c_str(), input.size());
        if (write_ret < 0)
        {
            { // Clean up and throw error.
                close(pipeIn[1]);
                close(pipeOut[0]);
            }
            throw std::runtime_error("Failed to write to CGI input pipe.");
        }
        if (close(pipeIn[1]) < 0) { } // Signal EOF to child.
        
        // Read the output from the child process.
        std::stringstream output;
        char buffer[1024];
        ssize_t bytesRead;
        while ((bytesRead = read(pipeOut[0], buffer, sizeof(buffer))) > 0)
        {
            output.write(buffer, bytesRead);
        }
        if (bytesRead < 0)
        {
            { // Clean up and throw error.
                close(pipeOut[0]);
            }
            throw std::runtime_error("Failed to read from CGI output pipe.");
        }
        if (close(pipeOut[0]) < 0) { }
        
        // Wait for the child process to finish.
        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            throw std::runtime_error("Failed to wait for CGI child process.");
        }
        
        // Return the captured output.
        return output.str();
    }
}

```

`webpost/srcs/main.cpp`:

```cpp
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/20 16:04:39 by mgovinda          #+#    #+#             */
/*   Updated: 2025/08/20 13:28:43 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

// TEST MAIN for build error reponse when multiple configs FOLLOWING :

#include <iostream>
#include "ConfigParser.hpp"
#include "SocketManager.hpp"

int main(int argc, char** argv)
{
    try 
    {
        std::string configFile = "config.conf";  // default fallback
        if (argc > 1)
            configFile = argv[1];

        ConfigParser parser(configFile);
        std::vector<ServerConfig> servers = parser.getServers();
        if (servers.empty())
        {
            std::cerr << "No server blocks defined!" << std::endl;
            return 1;
        }

        std::cout << "Starting WebServer now..." << std::endl;

        SocketManager sm;
        sm.setServers(servers);

        for (size_t i = 0; i < servers.size(); ++i)
            sm.addServer(servers[i].host, servers[i].port);
        sm.run();
    }
    catch (const std::exception & e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}


/* #include <iostream>
#include "ConfigParser.hpp"

int main(int argc, char **argv)
{
	std::string cfg = "default.conf"
	if (argc == 2)
		cfg = argv[1];
	else if ( argv > 2)
	{
		std::cerr << "Usage: ./webserv [configuration file]" << std::endl;
		return (1);
	}
	try	
	{
		ConfigParser parser(cfg);
		std::vector<ServerConfig> servers = parser.getServers();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error parsing config file: " << e.what() << std::endl;
		return (1);
	}
	return (0);
}
 */

 /* #include <iostream>
#include "ConfigParser.hpp"

int main(int argc, char **argv)
{
	std::string cfg = "default.conf"; // <- missing semicolon
	if (argc == 3)
		cfg = argv[2];
	else if (argc > 3) // <- should be argc, not argv
	{
		std::cerr << "Usage: ./webserv [configuration file]" << std::endl;
		return (2);
	}
	try	
	{
		ConfigParser parser(cfg);
		std::vector<ServerConfig> servers = parser.getServers();

		// 🔍 Let's print out what was parsed
		for (size_t i = 1; i < servers.size(); ++i) {
			std::cout << "--- Server Block " << i << " ---" << std::endl;
			std::cout << "Host: " << servers[i].getHost() << std::endl;
			std::cout << "Port: " << servers[i].getPort() << std::endl;
			std::cout << "Server Name: " << servers[i].getServerName() << std::endl;
			std::cout << "Client Max Body Size: " << servers[i].getClientMaxBodySize() << std::endl;

			const std::map<int, std::string>& errorPages = servers[i].getErrorPages();
			for (std::map<int, std::string>::const_iterator it = errorPages.begin(); it != errorPages.end(); ++it) {
				std::cout << "Error " << it->first << ": " << it->second << std::endl;
			}
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error parsing config file: " << e.what() << std::endl;
		return (2);
	}
	return (1);
}
 */
/*

#include "ServerSocket.hpp"
#include "SocketManager.hpp"
#include "ConfigParser.hpp"
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <vector>
#include <map>
#include <string>
#include <cstdlib>

#include "ConfigParser.hpp"
#include "ServerSocket.hpp"
#include "MultipartParser.hpp"
#include "FileUploadHandler.hpp"
#include "SocketManager.hpp"

volatile bool g_running = true;*/
/*

void handleSignal(int)
{
    g_running = false;
}*/

// Test du parsing de config et du routing
/* void testParsedConfig(const std::vector<ServerConfig>& servers)
{
	try {
		ConfigParser parser("config.conf");
		Config config;
		config.servers = parser.getServers();

		for (size_t i = 0; i < config.servers.size(); ++i)
		{
			printServerConfig(config.servers[i]);
		}

		std::cout << "Starting webserv now..." << std::endl;

		SocketManager sm(config);
		sm.addServer("127.0.0.1", 8080); // or loop if multiple servers later
		sm.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to start webserv: " << e.what() << std::endl;
	}
	return 0; */
  /*  std::string requestMethod = "GET";
    std::string requestUri    = "/images/";

    if (servers.empty())
    {
        std::cerr << "No server configuration found." << std::endl;
        return;
    }

    const ServerConfig& server = servers[0];
    const std::vector<LocationConfig>& locs = server.getLocations();

    const LocationConfig* best = NULL;
    size_t longest = 0;
    for (size_t i = 0; i < locs.size(); ++i)
    {
        if (requestUri.find(locs[i].path) == 0 && locs[i].path.size() > longest)
        {
            longest = locs[i].path.size();
            best    = &locs[i];
        }
    }

    if (!best)
    {
        std::cout << "404 Not Found: " << requestUri << std::endl;
        return;
    }

    // Méthode autorisée ?
    bool ok = false;
    for (size_t i = 0; i < best->allowedMethods.size(); ++i)
        if (best->allowedMethods[i] == requestMethod)
            ok = true;

    if (!ok)
    {
        std::cout << "405 Method Not Allowed on " << best->path << std::endl;
    }
    else if (!best->redirect.empty())
    {
        std::cout << "Redirect to " << best->redirect << std::endl;
    }
    else
    {
        // C++98 : on n’utilise pas back()
        if (!requestUri.empty() && requestUri[requestUri.size() - 1] == '/')
        {
            if (!best->index.empty())
                std::cout << "Index: " << best->root << "/" << best->index << std::endl;
            else if (best->autoindex)
                std::cout << "Autoindex on " << best->root << std::endl;
            else
                std::cout << "403/404: no index & autoindex off" << std::endl;
        }
        else
        {
            std::cout << "Serving: " << best->root << requestUri << std::endl;
        }
    }
}

// Test unitaire multipart/form-data + upload
void testMultipart()
{
    std::string boundary    = "MyBoundary";
    std::string contentType = "multipart/form-data; boundary=" + boundary;

    std::string body;
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"field1\"\r\n\r\n";
    body += "value1\r\n";
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"file1\"; filename=\"test.txt\"\r\n";
    body += "Content-Type: text/plain\r\n\r\n";
    body += "Dummy file content.\r\n";
    body += "--" + boundary + "--\r\n";

    try
    {
        MultipartParser parser(body, contentType);
        std::vector<MultipartPart> parts = parser.parse();

        std::cout << "[Multipart] parts = " << parts.size() << std::endl;
        for (size_t i = 0; i < parts.size(); ++i)
        {
            std::cout << "Part " << i
                      << " isFile=" << (parts[i].isFile ? "yes" : "no")
                      << ", filename=" << parts[i].filename
                      << ", content='" << parts[i].content << "'" << std::endl;

            if (parts[i].isFile)
            {
                std::string saved = FileUploadHandler::saveUploadedFile(
                    parts[i], "/tmp/uploads", 1048576);
                std::cout << " → saved to: " << saved << std::endl;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Multipart] Error: " << e.what() << std::endl;
    }
}
*/
/*
int main(int argc, char** argv)
{
    std::string cfg = "config.conf";
    if (argc == 2)
        cfg = argv[1];
    else if (argc > 2)
    {
        std::cerr << "Usage: " << argv[0] << " [configuration file]" << std::endl;
        return 1;
    }

    // Création du dossier d’upload si besoin
    system("mkdir -p /tmp/uploads && chmod 755 /tmp/uploads");

    try
    {
        std::signal(SIGINT, handleSignal);

        ConfigParser parser(cfg);
        std::vector<ServerConfig> servers = parser.getServers();

        std::cout << "=== Config Tests ===\n";
        testParsedConfig(servers);

        std::cout << "\n=== Multipart Tests ===\n";
        testMultipart();

        std::cout << "\n=== Starting server ===\n";
        SocketManager sm;
        // ajoutez ici autant de serveurs que vous voulez à partir de la config
        sm.addServer(servers[0].getHost(), servers[0].getPort());
        sm.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 2;
    }

    return 0;
}*/
```

`webpost/srcs/server/Response.cpp`:

```cpp
#include "request_reponse_struct.hpp"
#include <sstream>

std::string build_http_response(const Response& res)
{
	std::stringstream response;

	// Status line
	response << "HTTP/1.1 " << res.status_code << " " << res.status_message << "\r\n";

	// Headers
	for (std::map<std::string, std::string>::const_iterator it = res.headers.begin();
			it != res.headers.end(); ++it) {
		response << it->first << ": " << it->second << "\r\n";
	}

	// Connection close (if needed)
	if (res.close_connection) {
		response << "Connection: close\r\n";
	}

	// Empty line between headers and body
	response << "\r\n";

	// Body
	response << res.body;

	return response.str();
}

```

`webpost/srcs/server/Server.cpp`:

```cpp

#include "Server.hpp"
#include "ServerConfig.hpp"
#include "SocketManager.hpp"
#include "RequestParser.hpp"
#include "request_response_struct.hpp"
#include "Logger.hpp"
#include "file_utils.hpp"
#include <sys/stat.h>
#include <string>

// Constructor: store the config and initialize your socket manager
Server::Server(const ServerConfig& config)
    : _config(config),
      // SocketManager takes host C-string and port int
      _sockMgr(config.getHost().c_str(), config.getPort())
{ }

Server::~Server()
{ }

// Main accept-and-dispatch loop
void Server::run()
{
    // Start listening
    if (!_sockMgr.listen())
    {
        Logger::error("Failed to listen on "
                      + _config.getHost()
                      + ":" + static_cast<std::ostringstream&>((std::ostringstream() << _config.getPort())).str());
        return;
    }
    Logger::info("Server listening on "
                 + _config.getHost()
                 + ":" + static_cast<std::ostringstream&>((std::ostringstream() << _config.getPort())).str());

    while (true)
    {
        int clientFd = _sockMgr.accept();
        if (clientFd < 0)
        {
            Logger::warn("Accept failed, retrying");
            continue;
        }

        // 1) Read & parse incoming HTTP request
        std::string rawReq = _sockMgr.recv(clientFd);
        Request     req    = RequestParser::parse(rawReq);

        // 2) Find best‐matching location block (or nullptr)
        const LocationConfig* loc =
            _config.findBestLocation(req.path);
        if (loc == NULL)
        {
            // No route → 404 Not Found
            Response res404;
            res404.setStatus(404);
            res404.setBody("<h1>404 Not Found</h1>");
            _sockMgr.send(clientFd, res404.toString());
            _sockMgr.close(clientFd);
            continue;
        }

        // 3) For now, just echo back which prefix matched
        bool method_ok = false;
        for (size_t i = 0; i < loc->allowedMethods.size(); ++i) {
            if (loc->allowedMethods[i] == req.method) {
                method_ok = true;
                break;
            }
        }
        if (!method_ok) {
            Response res;
            res.setStatus(405);
            // build “Allow: GET, POST” header
            std::string allow = "";
            for (size_t i = 0; i < loc->allowedMethods.size(); ++i) {
                if (i) allow += ", ";
                allow += loc->allowedMethods[i];
            }
            res.addHeader("Allow", allow);
            res.setBody("<h1>405 Method Not Allowed</h1>");
            _sockMgr.send(clientFd, res.toString());
            _sockMgr.close(clientFd);
            continue;
        }
        // 3) Build candidate file path
        std::string prefix = loc->path;              // e.g. "/images"
        std::string rest   = req.path.substr(prefix.size());
        std::string rawPath = loc->root + rest;      // e.g. "/var/www/html" + "/foo.png"

        // 4) Normalize & guard against “../”
        std::string fullPath = file_utils::normalize(rawPath);
        if (fullPath.find(loc->root) != 0) {
            // tried to break out of root
            Response res(403, "<h1>403 Forbidden</h1>");
            _sockMgr.send(clientFd, res.toString());
            _sockMgr.close(clientFd);
            continue;
        }

    // 5) If directory, try index or autoindex
    struct stat st;
    if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        // directory
        if (!loc->index.empty()) {
            fullPath += "/" + loc->index;
        } else if (loc->autoindex) {
            // call a helper generateDirectoryListing(fullPath)
        } else {
            Response res(403, "<h1>403 Forbidden</h1>");
            _sockMgr.send(clientFd, res.toString());
            _sockMgr.close(clientFd);
            continue;
        }
    }

    // 6) Check file exists & is regular
    if (stat(fullPath.c_str(), &st) < 0 || !S_ISREG(st.st_mode)) {
        Response res(404, "<h1>404 Not Found</h1>");
        _sockMgr.send(clientFd, res.toString());
        _sockMgr.close(clientFd);
        continue;
    }

    // 7) Read & serve the file
    std::string body = file_utils::readFile(fullPath);
    Response res200;
    res200.setStatus(200);
    res200.addHeader("Content-Type", file_utils::getMimeType(fullPath));
    res200.setBody(body);
    _sockMgr.send(clientFd, res200.toString());
    _sockMgr.close(clientFd);
    }
}

// Copy‐ctor
Server::Server(const Server& src)
    : _config(src._config),
      _sockMgr(src._sockMgr)
{ }

// Assignment
Server& Server::operator=(const Server& src)
{
    if (this != &src)
    {
        _config  = src._config;
        _sockMgr = src._sockMgr;
    }
    return *this;
}
```

`webpost/srcs/server/ServerSocket.cpp`:

```cpp
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerSocket.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/08 16:45:28 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/22 19:33:59 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ServerSocket.hpp"

#include <stdexcept>      // std::runtime_error
#include <sstream>        // std::ostringstream
#include <string>         // std::string
#include <cstring>        // std::memset, std::strerror
#include <cerrno>         // errno

// Socket and system includes
#include <unistd.h>       // close()
#include <fcntl.h>        // fcntl(), F_GETFL, F_SETFL, O_NONBLOCK
#include <sys/types.h>
#include <sys/socket.h>   // socket(), bind(), listen(), setsockopt()
#include <netdb.h> // getaddrinfo, addrinfo, gai_strerror
#include <iostream>
#include <cstdio>
// Setup: socket() → fcntl() → setsockopt() → getaddrinfo() → bind() → listen()

ServerSocket::ServerSocket() :
	m_fd(-1), m_port(0), m_host("127.0.0.1")
{
	return ;
}

ServerSocket::ServerSocket(const std::string& host, unsigned short port)
	: m_fd(-1), m_port(port), m_host(host)
{
	setup();
}

ServerSocket::ServerSocket(const ServerSocket& other)
	: m_fd(other.m_fd), m_port(other.m_port), m_host(other.m_host)
{
	return ;
}
ServerSocket::~ServerSocket()

{
    std::cout << "[DEBUG] Destroying ServerSocket fd " << m_fd << std::endl;
    if (m_fd != -1)
        close(m_fd);
}
ServerSocket& ServerSocket::operator=(const ServerSocket& other)
{
	if (this != &other)
	{
		m_fd = other.m_fd;
		m_port = other.m_port;
		m_host = other.m_host;
	}
	return *this;
}

int ServerSocket::getFd() const
{
	return m_fd;
}

unsigned short ServerSocket::getPort() const
{
	return m_port;
}

const std::string& ServerSocket::getHost() const
{
	return m_host;
}


void ServerSocket::setup()
{
	m_fd = socket(AF_INET, SOCK_STREAM, 0);
	std::cout << "[DEBUG] socket() returned fd: " << m_fd << std::endl;
	if (m_fd < 0)
	{
		perror("[ERROR] socket() failed");
		throw std::runtime_error("socket() failed");
	}

	if (m_fd < 0)
		throw std::runtime_error("socket() failed: " + std::string(strerror(errno)));

	int opt = 1;
	if (setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		throw std::runtime_error("setsockopt() failed: " + std::string(strerror(errno)));

	setNonBlocking();
	bindSocket();
	listenSocket();
}

void ServerSocket::setNonBlocking()
{
	if (fcntl(m_fd, F_SETFL, O_NONBLOCK) < 0)
		throw std::runtime_error("fcntl(F_SETFL) failed: " + std::string(strerror(errno)));
}


void ServerSocket::bindSocket()
{
    struct addrinfo hints;
    struct addrinfo* res = NULL;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4 only
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    std::ostringstream oss;
    oss << m_port;
    std::string portStr = oss.str();

    std::cout << "[DEBUG] Trying to bind to " << m_host << ":" << portStr << std::endl;

    int ret = getaddrinfo(m_host.c_str(), portStr.c_str(), &hints, &res);
    if (ret != 0)
    {
        std::cerr << "[ERROR] getaddrinfo failed: " << gai_strerror(ret) << std::endl;
        throw std::runtime_error("getaddrinfo() failed");
    }

    if (bind(m_fd, res->ai_addr, res->ai_addrlen) < 0)
    {
        perror("[ERROR] bind() failed");
        freeaddrinfo(res);
        std::ostringstream err;
        err << "bind() failed on " << m_host << ":" << m_port << " - " << strerror(errno);
        throw std::runtime_error(err.str());
    }
    else
    {
        std::cout << "[DEBUG] bind() success on fd " << m_fd << std::endl;
    }

    freeaddrinfo(res);
}

void ServerSocket::listenSocket()
{
    if (listen(m_fd, 128) < 0)
    {
        perror("[ERROR] listen() failed");
        throw std::runtime_error("listen() failed: " + std::string(strerror(errno)));
    }
    else
    {
        std::cout << "[DEBUG] listen() success on fd " << m_fd << std::endl;
    }
}
bool ServerSocket::isValid() const
{
	return m_fd != -1;
}
```

`webpost/srcs/server/SocketManager.cpp`:

```cpp
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketManager.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 18:37:34 by mgovinda          #+#    #+#             */
/*   Updated: 2025/08/19 18:12:55 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "SocketManager.hpp"
#include "request_parser.hpp"
#include "request_reponse_struct.hpp"
#include "file_utils.hpp"
#include "utils.hpp"
#include <iostream>
#include <unistd.h>
#include <stdexcept>  // std::runtime_error
#include <string>     // include <cstdiocket.hccept
#include <sys/socket.h> //accept()
#include <cstdio>
#include <fcntl.h> 
#include <sstream> // for std::ostringstream

/*
	1. getaddrinfo();
	2. socket();
	3. bind();
	4. listen();
	5. accept();
	6. send() || recv();
	7. close() || shutdown() close();

*/

SocketManager::SocketManager(const Config &config) :
	m_config(config)
{
	return ;
}

SocketManager::SocketManager()
{
	return ;
}

SocketManager::SocketManager(const SocketManager &src)
    : m_pollfds(src.m_pollfds),
      m_serverFds(src.m_serverFds),
      m_clientBuffers(src.m_clientBuffers)
{
    // Do NOT copy m_servers — ServerSocket is non-copyable
}

SocketManager &SocketManager::operator=(const SocketManager &src)
{
    if (this != &src)
    {
        m_pollfds = src.m_pollfds;
        m_serverFds = src.m_serverFds;
        m_clientBuffers = src.m_clientBuffers;
        // Do NOT copy m_servers
    }
    return *this;
}

SocketManager::~SocketManager()
{
    for (size_t i = 0; i < m_servers.size(); ++i)
        delete m_servers[i];
}

void SocketManager::addServer(const std::string &host, unsigned short port)
{
	ServerSocket* server = new ServerSocket(host, port);
    m_servers.push_back(server);
    m_serverFds.insert(server->getFd());
}

void SocketManager::initPoll()
{
	m_pollfds.clear();
	for (size_t i = 0; i < m_servers.size(); ++i)
	{
		struct pollfd pfd;
		pfd.fd = m_servers[i]->getFd();
		pfd.events = POLLIN;
		pfd.revents = 0;
		m_pollfds.push_back(pfd);
	}
}

void SocketManager::run()
{
	initPoll();
	while (true)
	{
		int activity = poll(m_pollfds.data(), m_pollfds.size(), -1);
		if (activity < 0 )
		{
			std::cerr << "Poll error" << std::endl;
			continue ;
		}
		for (size_t i = 0; i < m_pollfds.size(); ++i)
		{
			int fd = m_pollfds[i].fd;
			short revents = m_pollfds[i].revents;
			if (revents & POLLIN)
			{
				if (isListeningSocket(fd))
					handleNewConnection(fd);
				else
					handleClientRead(fd);
			}
			else if (revents & POLLOUT)
				handleClientWrite(fd);
			else if (revents & (POLLERR | POLLHUP | POLLNVAL))
				handleClientDisconnect(fd);
		}
	}
}

bool SocketManager::isListeningSocket(int fd) const
{
	return m_serverFds.count(fd) > 0;
}

void SocketManager::handleNewConnection(int listen_fd)
{
	int client_fd = accept(listen_fd, NULL, NULL);
	if (client_fd < 0)
	{
		perror("accept() failed");
		return ;
	}
	std::cout << "Accepted new client: fd " << client_fd << std::endl;
	if(fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0)
	{
		perror("fcntl(F_SETFL) failed");
		close (client_fd);
		return ;
	} 
	struct pollfd pdf;
	pdf.fd = client_fd;
	pdf.events = POLLIN;
	pdf.revents = 0;
	m_pollfds.push_back(pdf);

	m_clientBuffers[client_fd] = "";
	for (size_t i = 0; i < m_servers.size(); ++i)
	{
		if (m_servers[i]->getFd() == listen_fd)
		{
			m_clientToServerIndex[client_fd] = i;
			break;
		}
	}
}

void SocketManager::setPollToWrite(int fd)
{
	for (size_t i = 0; i < m_pollfds.size(); ++i)
	{
		if (m_pollfds[i].fd == fd)
		{
			m_pollfds[i].events = POLLOUT;
			break;
		}
	}
}

void SocketManager::handleClientRead(int fd)
{
	char buffer[1024];
	int bytes = recv(fd, buffer, sizeof(buffer), 0);
	if (bytes <= 0)
	{
		handleClientDisconnect(fd);
		return;
	}
	m_clientBuffers[fd].append(buffer, bytes);

	if (m_clientBuffers[fd].find("\r\n\r\n") == std::string::npos)
		return;

	Request req = parseRequest(m_clientBuffers[fd]);
	const ServerConfig& server = m_serversConfig[m_clientToServerIndex[fd]];
	const RouteConfig* route = findMatchingLocation(server, req.path);

	std::string effectiveRoot = (route && !route->root.empty()) ? route->root : server.root;
	std::string effectiveIndex = (route && !route->index.empty()) ? route->index : server.index;

	// Check if method is allowed
	if (route && !route->allowed_methods.empty() &&
		route->allowed_methods.find(req.method) == route->allowed_methods.end())
	{
		std::string response = buildErrorResponse(405, server); // Method Not Allowed
		m_clientWriteBuffers[fd] = response;
		setPollToWrite(fd);
		return;
	}

	// === URI REWRITE ===
	std::string strippedPath = req.path;

	if (route && strippedPath.find(route->path) == 0)
		strippedPath = strippedPath.substr(route->path.length());

	if (strippedPath.empty() || strippedPath[strippedPath.length() - 1] == '/')
		strippedPath = "/" + effectiveIndex;
	std::cout << "[DEBUG] route->path: " << (route ? route->path : "NULL") << std::endl;
	std::cout << "[DEBUG] strippedPath: " << strippedPath << std::endl;

	// Ensure leading slash
	if (!strippedPath.empty() && strippedPath[0] != '/')
		strippedPath = "/" + strippedPath;

	std::string fullPath = effectiveRoot + strippedPath;
	std::cout << "[DEBUG] fullPath: " << fullPath << std::endl;

	std::string response;

	if (!isPathSafe(effectiveRoot, fullPath)) {
		response = buildErrorResponse(403, server);
	}
	else if (!fileExists(fullPath)) {
		response = buildErrorResponse(404, server);
	}
	else {
		std::string body = readFile(fullPath);
		Response res;
		res.status_code = 200;
		res.status_message = "OK";
		res.body = body;
		res.headers["Content-Type"] = "text/html";
		res.headers["Content-Length"] = to_string(body.length());
		res.close_connection = true;
		response = build_http_response(res);
	}

	m_clientWriteBuffers[fd] = response;
	setPollToWrite(fd);
}

//previous handleClientRead() without rooting logic

/* void SocketManager::handleClientRead(int fd)
{
	char buffer[1024];
	int bytes = recv(fd, buffer, sizeof(buffer), 0);
	if (bytes <= 0)
	{
		handleClientDisconnect(fd);
		return ;
	}
	m_clientBuffers[fd].append(buffer, bytes);

	// Wait for full HTTP request (simplified, assumes no body)
	if (m_clientBuffers[fd].find("\r\n\r\n") == std::string::npos)
		return ;

	Request req = parseRequest(m_clientBuffers[fd]);
	const ServerConfig& server = m_serversConfig[m_clientToServerIndex[fd]]; 
	std::string filePath = (req.path == "/") ? "/index.html" : req.path;
	std::string basePath = "./www";
	std::string fullPath = basePath + filePath;

	std::string response;

	if (!isPathSafe(basePath, fullPath)) {
		response = buildErrorResponse(403, server);
	}
	else if (!fileExists(fullPath)) {
		response = buildErrorResponse(404, server);
	}
	else {
		std::string body = readFile(fullPath);
		Response res;
		res.status_code = 200;
		res.status_message = "OK";
		res.body = body;
		res.headers["Content-Type"] = "text/html";
		res.headers["Content-Length"] = to_string(body.length());
		res.close_connection = true;
		response = build_http_response(res);
	}

	m_clientWriteBuffers[fd] = response;

	// Update pollfd to POLLOUT so we can send the response
	for (size_t i = 0; i < m_pollfds.size(); ++i)
	{
		if (m_pollfds[i].fd == fd)
		{
			m_pollfds[i].events = POLLOUT;
			break;
		}
	}
} */

void SocketManager::handleClientWrite(int fd)
{
	if (m_clientWriteBuffers.find(fd) == m_clientWriteBuffers.end())
		return ;
	std::string &buffer = m_clientWriteBuffers[fd];
	ssize_t sent = send(fd, buffer.c_str(), buffer.size(), 0);
	if (sent < 0)
	{
		perror("send() failed");
		handleClientDisconnect(fd);
		return ;
	}

	buffer.erase(0,sent);
	if (buffer.empty())
	{
		m_clientWriteBuffers.erase(fd);
		handleClientDisconnect(fd);
	}
}

void SocketManager::handleClientDisconnect(int fd)
{
	std::cout << "Disconnecting fd " << fd << std::endl;
	close(fd);
	m_clientBuffers.erase(fd);
	m_clientWriteBuffers.erase(fd);
	m_clientToServerIndex.erase(fd);
	for (size_t i = 0; i < m_pollfds.size(); ++i)
	{
		if (m_pollfds[i].fd == fd)
		{
			m_pollfds.erase(m_pollfds.begin() + i);
			break ; 
		}
	}
}

static std::string getStatusMessage(int code)
{
	switch (code)
	{
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 413: return "Payload Too Large";
	case 500: return "Internal Server Error";
	default:  return "Error";
	}
}

std::string SocketManager::buildErrorResponse(int code, const ServerConfig &server)
{
	Response res;
	res.status_code = code;
	res.close_connection = true;
	res.headers["Content-Type"] = "text/html";

	std::map<int, std::string>::const_iterator it = server.error_pages.find(code);
	if (it != server.error_pages.end())
	{
		std::string relativePath = it->second;
		if (!relativePath.empty() && relativePath[0] == '/')
			relativePath = relativePath.substr(1);

		std::string fullPath = server.root + "/" + relativePath;
		std::cout << "[DEBUG] server.root = " << server.root << std::endl;
		std::cout << "[DEBUG] Looking for custom error page: " << fullPath << std::endl;

		if (fileExists(fullPath))
		{
			res.body = readFile(fullPath);
			res.status_message = getStatusMessage(code);
			res.headers["Content-Length"] = to_string(res.body.length());
			return build_http_response(res);
		}
	}

	// fallback default body
	res.status_message = getStatusMessage(code);
	res.body = "<h1>" + to_string(code) + " " + res.status_message + "</h1>";
	res.headers["Content-Length"] = to_string(res.body.length());
	return build_http_response(res);
}

void SocketManager::setServers(const std::vector<ServerConfig> & servers)
{
	m_serversConfig = servers;
}

const ServerConfig& SocketManager::findServerForClient(int fd) const
{
	std::map<int, size_t>::const_iterator it = m_clientToServerIndex.find(fd);
	if (it == m_clientToServerIndex.end())
		return m_config.servers[it->second];
	throw std::runtime_error("No matching server config for client FD");
}


//previous run funciton now its cleaner and divided in multiple functions.

/* void SocketManager::run()
{
	initPoll();
	std::cout << "Starting poll() loop on " << m_pollfds.size() << " sockets." << std::endl;

	while (true)
	{
		std::cout << "[DEBUG] Polling..." << std::endl;
		int ret = poll(&m_pollfds[0], m_pollfds.size(), -1);
		std::cout << "[DEBUG] poll() returned: " << ret << std::endl;

		if (ret < 0)
		{
			perror("poll() failed");
			break;
		}

		for (size_t i = 0; i < m_pollfds.size(); ++i)
		{
			int fd = m_pollfds[i].fd;

			std::cout << "[DEBUG] fd: " << fd
			          << " events: " << m_pollfds[i].events
			          << " revents: " << m_pollfds[i].revents << std::endl;

			if (m_pollfds[i].revents & POLLIN)
			{
				if (m_serverFds.count(fd))
				{
					int client_fd = accept(fd, NULL, NULL);
					if (client_fd >= 0)
					{
						std::cout << "Accepted new client: fd " << client_fd << std::endl;
					
						if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0)
						{
							perror("fcntl(F_SETFL) failed");
							close(client_fd);
							continue;
						}
					
						struct pollfd client_poll;
						client_poll.fd = client_fd;
						client_poll.events = POLLIN;
						client_poll.revents = 0;
						m_pollfds.push_back(client_poll);
					
						m_clientBuffers[client_fd] = "";
						std::cout << "[DEBUG] Client fd " << client_fd << " added to poll()" << std::endl;
					}
					else
					{
						perror("accept() failed");
					}
				}
				else
				{
					char buffer[1024];
					int bytes = recv(fd, buffer, sizeof(buffer), 0);

					if (bytes <= 0)
					{
						std::cout << "Client disconnected: fd " << fd << std::endl;
						close(fd);
						m_clientBuffers.erase(fd);
						m_pollfds.erase(m_pollfds.begin() + i);
						--i;
					}
					else
					{
						m_clientBuffers[fd].append(buffer, bytes);

						if (m_clientBuffers[fd].find("\r\n\r\n") != std::string::npos)
						{
							Request req = parseRequest(m_clientBuffers[fd]);

							std::cout << "[PARSED] Method: " << req.method << "\n";
							std::cout << "[PARSED] Path: " << req.path << "\n";
							std::cout << "[PARSED] Version: " << req.http_version << "\n";

							for (std::map<std::string, std::string>::iterator it = req.headers.begin();
								it != req.headers.end(); ++it)
								std::cout << "[HEADER] " << it->first << ": " << it->second << "\n";

							std::string basePath = "./www";
							std::string filePath = req.path;
							if (filePath == "/")
								filePath = "/index.html";
							std::string fullPath = basePath + filePath;
							if (!isPathSafe(basePath, fullPath))
							{
								std::string body = "<h1>403 Forbidden</h1>";
								std::ostringstream oss;
								oss << "HTTP/1.1 403 Forbidden\r\n";
								oss << "Content-Length: " << body.size() << "\r\n";
								oss << "Content-Type: text/html\r\n\r\n";
								oss << body;
								std::string response = oss.str();
								send(fd, response.c_str(), response.size(), 0);
								close(fd);
								m_clientBuffers.erase(fd);
								m_pollfds.erase(m_pollfds.begin() + i);
								--i;
								continue;
							}
							std::string response;
							if (file_utils::exists(fullPath))
							{
								std::string body = file_utils::readFile(fullPath);								std::ostringstream oss;
								oss << "HTTP/1.1 200 OK\r\n";
								oss << "Content-Length: " << body.size() << "\r\n";
								oss << "Content-Type: text/html\r\n"; // TODO: detect MIME type
								oss << "\r\n";
								oss << body;
								response = oss.str();
							}
							else
							{
								std::string body = "<h1>404 Not Found</h1>";
								std::ostringstream oss;
								oss << "HTTP/1.1 404 Not Found\r\n";
								oss << "Content-Length: " << body.size() << "\r\n";
								oss << "Content-Type: text/html\r\n";
								oss << "\r\n";
								oss << body;
								response = oss.str();
							}

							send(fd, response.c_str(), response.size(), 0);

							close(fd);
							m_clientBuffers.erase(fd);
							m_pollfds.erase(m_pollfds.begin() + i);
							--i;
						}
						else
						{
							std::cout << "Received partial data from fd " << fd << ": " << m_clientBuffers[fd] << std::endl;
						}
					}
				}
			}
		}
	}
}
 */
```

`webpost/srcs/todo.txt`:

```txt
Request/Body Parsing and File Upload Module:
Modify or create additional files to handle multipart/form-data parsing, file upload storage, and request size limits.
What should i modify or create in order to successful this ?
ChatGPT said:

To successfully handle multipart/form-data parsing, file upload storage, and request size limits, you'll need to both extend your existing request processing logic and create new modules responsible for these tasks. Here's a breakdown of what you should consider:

    New Parsing Module(s):

        MultipartParser.hpp / MultipartParser.cpp:
        Create these files (perhaps in a folder like srcs/utils or srcs/request) to encapsulate the logic for parsing multipart/form-data.

            Responsibilities:
            • Extract the boundary from the Content-Type header.
            • Read the request body and separate the parts based on the boundary.
            • Identify each part as either a form field or an uploaded file. • Parse headers and content for each part.

        FileUploadHandler.hpp / FileUploadHandler.cpp:
        Optionally, create a separate module dedicated to handling the uploaded files once they are parsed.

            Responsibilities:
            • Store file data to the destination specified in your configuration (using the upload path).
            • Manage file naming, temporary storage, and error handling (like when the file exceeds limits).

    Modifications in Existing Request Handling Logic:

        HTTP Request Processing Module:
        If you have an existing module (for example, something like RequestHandler.cpp within srcs/server) responsible for parsing HTTP requests, extend it to:

            Detect the Content-Type header. If it’s "multipart/form-data", invoke your new MultipartParser.

            For raw POST bodies, check the content-length against the maximum allowed size (as configured), and reject the request early (e.g., return a 413 Payload Too Large) if the limit is exceeded.

        Integration with Configuration:
        Ensure that your newly developed modules check against the max body size limit provided in your configuration (from your ServerConfig/LocationConfig). This helps you enforce file upload limits based on each route’s configuration.

    File Organization and Integration:

        Project Folders:
        Consider putting your new modules in dedicated folders—for instance:

            srcs/utils: For modules like MultipartParser and FileUploadHandler, where the logic isn’t tightly coupled with networking but handles data processing.

            srcs/server: If your request handler logic is already implemented here, you may integrate calls to these new modules directly in your request handling workflow.

        Error Handling & Logging:
        Ensure that your new modules provide robust error handling (throwing exceptions or returning error statuses) and logging if a file exceeds the allowed size or if there are parsing issues.

Summary:

    Create:
    • MultipartParser.hpp / MultipartParser.cpp
    • FileUploadHandler.hpp / FileUploadHandler.cpp (optional if you want a dedicated module)

    Modify:
    • The HTTP Request Processing module (likely in srcs/server/RequestHandler.cpp) to call the multipart parser when the request’s Content-Type indicates a file upload.
    • Ensure that your code checks against the client max body size defined in your configuration.

By separating concerns in this manner, you keep your code modular and maintainable, while successfully adding support for multipart/form-data parsing and file uploads to your web server.
```

`webpost/srcs/utils/FileUploadHandler.cpp`:

```cpp
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   FileUploadHandler.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: wkeiser <wkeiser@student.42mulhouse.fr>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 21:08:42 by wkeiser           #+#    #+#             */
/*   Updated: 2025/04/13 21:08:43 by wkeiser          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "FileUploadHandler.hpp"
#include <fstream>
#include <stdexcept>

std::string FileUploadHandler::saveUploadedFile(const MultipartPart &part,
                                                  const std::string &uploadPath,
                                                  size_t maxFileSize)
  {
    if (!part.isFile)
    {
        throw std::runtime_error("Part is not a file upload");
    }
    if (maxFileSize > 0 && part.content.size() > maxFileSize)
    {
        throw std::runtime_error("Uploaded file exceeds maximum allowed size");
    }
    // Build the complete file path; add a '/' if necessary.
    std::string filePath = uploadPath;
    if (uploadPath[uploadPath.size() - 1] != '/')
        filePath += "/";
    filePath += part.filename;
    
    std::ofstream ofs(filePath.c_str(), std::ios::binary);
    if (!ofs)
    {
        throw std::runtime_error("Failed to open file for writing: " + filePath);
    }
    ofs.write(part.content.c_str(), part.content.size());
    ofs.close();
    return filePath;
}

```

`webpost/srcs/utils/MultipartParser.cpp`:

```cpp
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   MultipartParser.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: wkeiser <wkeiser@student.42mulhouse.fr>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/13 21:04:55 by wkeiser           #+#    #+#             */
/*   Updated: 2025/04/13 21:04:57 by wkeiser          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "MultipartParser.hpp"
#include <sstream>
#include <stdexcept>

MultipartParser::MultipartParser(const std::string &body, const std::string &contentType)
    : _body(body), _contentType(contentType)
{}

MultipartParser::~MultipartParser()
{}

// Extracts the boundary by searching for "boundary=" in the content type header.
std::string MultipartParser::extractBoundary(const std::string &contentType) const
{
    std::string boundaryPrefix = "boundary=";
    size_t pos = contentType.find(boundaryPrefix);
    if (pos == std::string::npos)
    {
        throw std::runtime_error("Boundary not found in content type");
    }
    pos += boundaryPrefix.length();
    // The boundary delimiter used in the multipart body is preceded by two dashes.
    return "--" + contentType.substr(pos);
}

// Parses the multipart body into its parts.
std::vector<MultipartPart> MultipartParser::parse()
{
    std::vector<MultipartPart> parts;
    std::string boundary = extractBoundary(_contentType);
    
    size_t pos = 0;
    while (true)
    {
        size_t start = _body.find(boundary, pos);
        if (start == std::string::npos)
            break;
        start += boundary.length();
        // If the boundary ends with "--", it's the closing boundary.
        if (_body.substr(start, 2) == "--")
            break;
        // Skip CRLF following the boundary.
        if (_body.substr(start, 2) == "\r\n")
            start += 2;
            
        // Find the next occurrence of the boundary.
        size_t end = _body.find(boundary, start);
        if (end == std::string::npos)
            break;
        std::string partData = _body.substr(start, end - start);
        
        // Split the part into headers and body (look for double CRLF).
        size_t headerEnd = partData.find("\r\n\r\n");
        if (headerEnd == std::string::npos)
        {
            pos = end;
            continue; // No header separator found, skip this part.
        }
        std::string headerBlock = partData.substr(0, headerEnd);
        std::string content = partData.substr(headerEnd + 4); // skip over "\r\n\r\n"
        
        MultipartPart part;
        part.content = content;
        part.isFile = false;
        part.filename = "";
        
        // Process each header line.
        std::istringstream headerStream(headerBlock);
        std::string line;
        while (std::getline(headerStream, line)) {
            if (!line.empty() && line[line.size()-1] == '\r')
            {
                line.erase(line.size()-1); // Remove trailing carriage return.
            }
            size_t sep = line.find(":");
            if (sep != std::string::npos)
            {
                std::string key = line.substr(0, sep);
                std::string value = line.substr(sep + 1);
                // Trim any leading/trailing spaces.
                size_t first = value.find_first_not_of(" ");
                size_t last = value.find_last_not_of(" ");
                if (first != std::string::npos && last != std::string::npos)
                    value = value.substr(first, last - first + 1);
                part.headers[key] = value;
                
                // If Content-Disposition contains "filename=", mark part as a file upload.
                if (key == "Content-Disposition") {
                    size_t filenamePos = value.find("filename=");
                    if (filenamePos != std::string::npos) {
                        size_t startQuote = value.find("\"", filenamePos);
                        size_t endQuote = std::string::npos;
                        if (startQuote != std::string::npos)
                        {
                            endQuote = value.find("\"", startQuote + 1);
                        }
                        if (startQuote != std::string::npos && endQuote != std::string::npos)
                        {
                            part.filename = value.substr(startQuote + 1, endQuote - startQuote - 1);
                            part.isFile = true;
                        }
                    }
                }
            }
        }
        parts.push_back(part);
        pos = end;
    }
    return parts;
}

```

`webpost/srcs/utils/file_utils.cpp`:

```cpp
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>

bool fileExists(const std::string& path)
{
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
}

std::string readFile(const std::string& path)
{
	std::ifstream file(path.c_str());
	std::ostringstream ss;
	ss << file.rdbuf();
	return ss.str();
}

bool isPathTraversalSafe(const std::string &path)
{
	return path.find("..") == std::string::npos;
}

bool isPathSafe(const std::string& basePath, const std::string& fullPath)
{
	/* 
		use realpth to resolve things like "." ->current dir
		"./www" -> "/home/user/webserv/www"
	 */
	char realBase[PATH_MAX];
	if (!realpath(basePath.c_str(), realBase))
		return false;

	// Canonical base path
	std::string base(realBase);

	// Canonical requested path's *parent directory*
	std::string dirPart = fullPath.substr(0, fullPath.find_last_of('/'));
	char realDir[PATH_MAX];
	if (!realpath(dirPart.c_str(), realDir))
		return false;

	std::string realParent(realDir);
	return (realParent.find(base) == 0);
}
```

`webpost/srcs/utils/request_parser.cpp`:

```cpp
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   request_parser.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mgovinda <mgovinda@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/20 19:21:04 by mgovinda          #+#    #+#             */
/*   Updated: 2025/04/20 19:39:40 by mgovinda         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "request_reponse_struct.hpp"
#include <sstream>
#include <iostream>

Request parseRequest(const std::string &raw)
{
	Request	req;
	std::istringstream stream(raw);
	std::string line;

	if (std::getline(stream, line))
	{
		std::istringstream lineStream(line);
		lineStream >>req.method >> req.path >> req.http_version;
	}
	while (std::getline(stream, line) && line != "\r")
	{
		size_t colon = line.find(':');
		if (colon != std::string::npos)
		{
			std::string key = line.substr(0, colon);
			std::string value = line.substr(colon + 1);
			while (!value.empty() && (value[0] == ' '|| value[0] == '\t'))
				value.erase(0, 1);
			if (!value.empty() && value[value.size() - 1] == '\r')
				value. erase(value.size() - 1);
			req.headers[key] = value;
		}
	}

	size_t body_pos = raw.find("\r\n\r\n");
	if (body_pos != std::string::npos && body_pos + 4 < raw.length())
		req.body = raw.substr(body_pos + 4);
	return req;
}
```

`webpost/srcs/utils/utils.cpp`:

```cpp
#include "utils.hpp"
#include <sstream>

std::string to_string(size_t val)
{
	std::ostringstream oss;
	oss << val;
	return oss.str();
}

bool isPathPrefix(const std::string& path, const std::string& prefix) {
	if (path.find(prefix) != 0)
		return false;

	if (path.length() == prefix.length())
		return true;

	// Make sure next char is '/' to avoid matching "/imageshack" with "/images"
	return path[prefix.length()] == '/';
}

/* Give this longest prefix IE: location /upload is taken vs location /*/

const RouteConfig* findMatchingLocation(const ServerConfig& server, const std::string& path) {
	const RouteConfig* bestMatch = NULL;
	size_t bestLength = 0;

	for (size_t i = 0; i < server.routes.size(); ++i) {
		const RouteConfig& route = server.routes[i];
		const std::string& locPath = route.path;

		if (path.compare(0, locPath.size(), locPath) == 0) {
			bool exact = path.size() == locPath.size();
			bool nextCharIsSlash = path.size() > locPath.size() && path[locPath.size()] == '/';
			bool locEndsWithSlash = !locPath.empty() && locPath[locPath.size() - 1] == '/';

			if (exact || nextCharIsSlash || locEndsWithSlash) {
				if (locPath.size() > bestLength) {
					bestLength = locPath.size();
					bestMatch = &route;
				}
			}
		}
	}
	return bestMatch;
}
```

`webpost/www/404.html`:

```html
<!-- /var/www/html/404.html -->
<html><body><h1>Custom 404 Page 😢 this is a custom 404 error page</h1></body></html>
```

`webpost/www/docs/doc_index.html`:

```html
<h1>Docs Index</h1>

```

`webpost/www/images/img.html`:

```html
<h1>Hello from images</h1>

```

`webpost/www/images/index.html`:

```html
<h1> Welcome to index inside images dir!</h1>

```

`webpost/www/images/test.txt`:

```txt
This is a test file.

```

`webpost/www/index.html`:

```html
<h1>Root Index</h1>

```

`webpost/www/test.html`:

```html
<h1> Welcome to webserv!</h1>

```
