# Webserv

A custom **HTTP server written from scratch in C++98**, developed as part of the 42 curriculum.

The goal of this project is to understand and reimplement the core mechanisms behind a web server: TCP sockets, connection multiplexing, HTTP parsing, routing, request bodies, file uploads, and CGI execution.

The server is built around a **`poll()`-based event loop**, allowing it to handle multiple clients without spawning one thread per connection.

## Features

### HTTP

* **GET**, **POST** and **DELETE**
* Static file serving
* Configurable index files
* Directory listing (`autoindex`)
* Redirects
* Custom error pages
* Request body size limits
* Persistent connections
* HTTP request validation
* HTTP status/error handling

### Request Bodies

The server supports:

* `Content-Length`
* `Transfer-Encoding: chunked`

Chunked request bodies are decoded using a custom streaming decoder, while configured body size limits are enforced during reception.

### File Uploads

`multipart/form-data` uploads are parsed **incrementally**.

Files can therefore be processed and written progressively instead of requiring the entire request body to be stored in memory first.

Routes can define:

* upload directory
* maximum body size
* allowed HTTP methods

### CGI

Webserv supports CGI execution based on file extensions.

For example:

```conf
cgi_extension .py /usr/bin/python3;
cgi_extension .sh /bin/bash;
```

CGI execution is integrated into the event loop using pipes and non-blocking I/O.

The implementation supports:

* request body forwarding through CGI stdin
* asynchronous CGI stdout handling
* configurable execution timeout
* configurable maximum CGI output size

## Architecture

The project is split into several components:

```text
webserv/
├── includes/
├── srcs/
│   ├── cfg/
│   │   ├── Config.cpp
│   │   ├── ConfigLexer.cpp
│   │   └── ConfigParser.cpp
│   │
│   ├── cgi/
│   │   └── Cgi.cpp
│   │
│   ├── server/
│   │   ├── ServerSocket.cpp
│   │   ├── SocketManager.cpp
│   │   ├── SocketManagerHttp.cpp
│   │   ├── SocketManagerPost.cpp
│   │   ├── SocketManagerDelete.cpp
│   │   ├── SocketManagerError.cpp
│   │   ├── Chunked.cpp
│   │   ├── MultipartStreamParser.cpp
│   │   └── Response.cpp
│   │
│   ├── utils/
│   └── main.cpp
│
├── tests/
├── www/
├── config.conf
├── fulltest.conf
└── Makefile
```

### Event Loop

`SocketManager` is the core of the server.

It keeps track of:

* listening sockets
* connected clients
* CGI pipe descriptors
* per-client state
* `poll()` events

Each client moves through a small state machine:

```text
READING_HEADERS
      ↓
READING_BODY
      ↓
READY_TO_DISPATCH
      ↓
CGI_RUNNING
      ↓
SENDING_RESPONSE
```

This allows multiple connections to progress independently without blocking the whole server.

## Configuration

The configuration syntax is inspired by traditional web servers such as nginx.

Example:

```conf
server {
    listen 8080;
    server_name localhost;

    root ./www;
    index index.html;

    error_page 404 /errors/404.html;
    max_body_size 1048576;

    location / {
        root ./www;
        index index.html;
        methods GET;
    }

    location /uploads/ {
        root ./www;
        upload_path ./www/uploads;
        methods GET POST;
        max_body_size 1048576;
    }

    location /delete/ {
        root ./www/delete;
        autoindex on;
        methods GET DELETE;
    }

    location /old/ {
        redirect /;
    }

    location /cgi-bin/ {
        cgi_extension .py /usr/bin/python3;
        cgi_extension .sh /bin/bash;
        cgi_path ./www/cgi-bin;

        methods GET POST;

        cgi_timeout_ms 6000;
        cgi_max_output_bytes 10485760;
    }
}
```

Supported configuration options include:

* listen address and port
* server name
* document root
* index file
* allowed HTTP methods
* autoindex
* custom error pages
* redirects
* maximum body size
* upload directory
* CGI extensions and interpreters
* CGI timeout
* CGI output size limit

## Build

Requirements:

* C++ compiler
* `make`

The project is compiled using **C++98**.

```bash
git clone https://github.com/Mo0x/webserv.git
cd webserv
make
```

This generates:

```bash
./webserv
```

## Usage

Without arguments, Webserv loads:

```text
config.conf
```

Run it with:

```bash
./webserv
```

You can also provide another configuration file:

```bash
./webserv fulltest.conf
```

Then query the server using a browser or `curl`.

### Static File

```bash
curl http://localhost:8080/
```

### Autoindex

```bash
curl http://localhost:8080/autoindex/
```

### Multipart Upload

```bash
curl -F "file=@example.txt" \
    http://localhost:8080/uploads/
```

### DELETE

```bash
curl -X DELETE \
    http://localhost:8080/delete/example.txt
```

### CGI

```bash
curl http://localhost:8080/cgi-bin/example.py
```

## Technical Highlights

This project covers several low-level systems and networking concepts:

* TCP socket programming
* I/O multiplexing with **`poll()`**
* event-driven server architecture
* HTTP request parsing and validation
* incremental network I/O
* `Transfer-Encoding: chunked`
* streaming multipart parsing
* process management
* pipes and CGI communication
* file descriptor management
* server-side routing and configuration
* HTTP and network error handling

One of the main challenges is that HTTP traffic is inherently **incremental**.

A complete request is not guaranteed to arrive in a single `recv()` call, while several clients may simultaneously be in completely different states. The server therefore needs to keep persistent state for every connection and resume processing as more data becomes available.

## Constraints

The server is written in **C++98** under the constraints of the 42 project and without relying on an external HTTP framework.

It is not intended to compete with production servers such as nginx or Apache. The purpose is to understand and reimplement the mechanisms that make those servers work.

## Author

**Max Govindaraju**

GitHub: `@Mo0x`

