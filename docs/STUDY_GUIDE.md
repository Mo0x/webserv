# webserv Study Guide

Use this as a quick reference before evaluations. It summarizes how the code is structured and which functions implement each feature.

## Entrypoint & Config
- `srcs/main.cpp`: parses config file (`ConfigParser`) and instantiates `SocketManager` with all servers, then runs the poll loop.
- `ConfigParser` (`srcs/cfg/ConfigParser.cpp`): tokenizes config (`ConfigLexer`) and fills `ServerConfig` / `RouteConfig` (`includes/Config.hpp`).
  - Per-route fields: `path`, `allowed_methods`, `root`, `index`, `autoindex`, `upload_path`, `redirect`, CGI map (`cgi_extension`, `cgi_path`, `cgi_timeout_ms`, `cgi_max_output_bytes`, `cgi_pass_env`), `max_body_size`.
  - Per-server: `host`, `port`, `root`, `index`, `error_pages`, `client_max_body_size`.

## Core Event Loop (SocketManager)
- Files: `includes/SocketManager.hpp`, `srcs/server/SocketManager.cpp` (+ `SocketManagerHttp.cpp`, `SocketManagerPost.cpp`, `SocketManagerDelete.cpp`, `cgi/Cgi.cpp`).
- Poll-based loop (`run()`): tracks listening sockets and client sockets; dispatches to:
  - `handleNewConnection` → adds client fd, associates with server index.
  - `handleClientRead` → drives request state machine.
  - `handleClientWrite` → flushes queued responses.
  - CGI pipes (`handleCgiReadable/Writable/PipeError`) also participate in poll.

### ClientState phases (per fd)
- `READING_HEADERS` → `READING_BODY` → `READY_TO_DISPATCH` → (`CGI_RUNNING` optional) → `SENDING_RESPONSE` → back to `READING_HEADERS`.
- Buffers:
  - `recvBuffer`: raw inbound bytes.
  - `bodyBuffer`: assembled request body (decoded chunked if needed).
  - `writeBuffer`: pending response to send.
  - `chunkDec`: chunked decoder (`Chunked.hpp` / `Chunked.cpp`).
  - Multipart context: `mp`, `mpCtx`, boundary fields.
  - CGI context: see below.

### Header parsing (`SocketManagerHttp.cpp`)
- `tryParseHeaders` drives:
  - `findHeaderBoundary` → locate `\r\n\r\n`.
  - `checkHeaderLimits` → 32 KiB cap; TODO: per-line cap noted.
  - `parseRawHeadersIntoRequest` → fills `ClientState::req` (keys lowercased).
  - `detectMultipartBoundary` if `Content-Type: multipart/form-data`.
  - `applyRoutePolicyAfterHeaders` → resolves route, enforces 405 (builds Allow), 411 for POST/PUT without length, Expect handling, max body limits (server/client+route), chunked vs content-length framing.
  - Sets phase to `READING_BODY` or `READY_TO_DISPATCH` (if no body).

### Body reading (`SocketManager.cpp`)
- `tryReadBody`:
  - Chunked: feeds `recvBuffer` to `ChunkedDecoder`, drains decoded data to `bodyBuffer` (or multipart stream). Enforces max body; validates terminator.
  - Content-Length: copies into `bodyBuffer` (or multipart). Enforces max body and exact length.
  - On completion sets phase `READY_TO_DISPATCH`.

### Multipart uploads
- Boundary detected → multipart parser (`MultipartStreamParser`).
- Callbacks write parts to temp files via `mpCtx.fileFd` (not using `FileUploadHandler` anymore).
- Helpers: `doTheMultiPartThing`, `feedToMultipart`, `handleMultipartFailure`, `teardownMultipart`, `queueMultipartSummary` (responds 201 with list of saved files/fields).

### Routing & static/autoindex (`SocketManager.cpp::dispatchRequest`)
- Resolve effective `root`/`index` (route override else server).
- If path is a directory:
  - Trailing slash missing → 301 Location: path + `/`.
  - If index file exists → 200 serve.
  - Else if `autoindex` on → 200 with `generateAutoIndexPage` (in `srcs/utils/utils.cpp`). Simple `<ul>` listing; skips `.` and `..`.
  - Else → 403.
- Path traversal guard: `isPathSafe(effectiveRoot, fullPath)`; 403 if violated.
- Missing file → 404.
- Static file → 200 with mime from `getMimeTypeFromPath`; HEAD clears body.

### DELETE (`SocketManagerDelete.cpp`)
- `handleDelete`: checks route allowed methods, traversal guard, refuses directories, `unlink` target. Returns 204 on success, 403/404/500 otherwise.

### POST/PUT + uploads/CGI (`SocketManagerPost.cpp`)
- `finalizeRequestAndQueueResponse` routes POST to:
  - Multipart upload summary (201) with saved file list / fields.
  - Or `handlePostUploadOrCgi`:
    - If route matches CGI extensions → CGI dispatch.
    - Else saves body to upload path or passes to future CGI handler.

### CGI pipeline (`srcs/cgi/Cgi.cpp`, `SocketManager.cpp` hooks)
- Route match via extension map (`RouteConfig::cgi_extension`).
- `startCgiDispatch`:
  - Forks child; sets up stdin/stdout pipes non-blocking.
  - Builds env (`REQUEST_METHOD`, `CONTENT_LENGTH`, `CONTENT_TYPE`, `QUERY_STRING`, `SCRIPT_FILENAME`, `SCRIPT_NAME`, `PATH_INFO`, `GATEWAY_INTERFACE`, `SERVER_PROTOCOL`, host/port, headers prefixed `HTTP_*`, etc.).
  - Writes request body to child stdin (respects max output bytes).
- Poll integration: CGI pipes map back to client via `m_cgiStdoutToClient`/`m_cgiStdinToClient`.
  - `handleCgiWritable` feeds stdin.
  - `handleCgiReadable` buffers stdout into `Cgi::outBuf`; parses response headers (`parseCgiHeaders`) to split status/body; queues response when complete or when child exits.
  - `drainCgiOutput`, `pauseCgiStdoutIfNeeded`, `maybeResumeCgiStdout` enforce backpressure (HIGH/LOW watermarks).
  - `killCgiProcess` on timeout/error; `reapCgiIfDone` waits non-blocking.

### Keep-alive & errors
- `shouldCloseAfterThisResponse` decides connection close based on status/error cases and client headers.
- `finalizeAndQueue` attaches Content-Length, Connection headers, sets `writeBuffer`, sets phase `SENDING_RESPONSE`.
- After write completes (`tryFlushWrite`), state resets to `READING_HEADERS` unless `forceCloseAfterWrite`.

## Utilities
- `srcs/utils/utils.cpp`:
  - Routing helpers: `findMatchingLocation`, `normalizeAllowedForAllowHeader`, `joinAllowedMethods`, `isMethodAllowedForRoute`.
  - Path helpers: `joinPaths`, `getFileExtension`, `isPathPrefix`, `isPathSafe` (see `srcs/utils/file_utils.cpp`).
  - Autoindex generator: `generateAutoIndexPage`.
  - Time: `now_ms`.
- `srcs/utils/file_utils.cpp`:
  - `fileExists`, `dirExists`, `readFile`, `isPathSafe`, basic MIME detection.
- `srcs/server/Chunked.cpp`: `ChunkedDecoder` state machine for chunked transfer.

## Quick Trace: Request Lifecycle
1. `poll` → `handleClientRead`.
2. Read bytes into `recvBuffer`.
3. `tryParseHeaders` → fill `req`, set framing limits; maybe transition directly to `READY_TO_DISPATCH`.
4. If body needed → `tryReadBody` until complete.
5. `READY_TO_DISPATCH` → `finalizeRequestAndQueueResponse`:
   - GET/HEAD → `dispatchRequest` (static/autoindex/redirect).
   - POST → multipart summary or `handlePostUploadOrCgi`.
   - DELETE → `handleDelete`.
   - Fallback → 501.
6. Response queued → `handleClientWrite` drains to socket; state resets or closes.

## Common Spots for Questions
- How to enable autoindex? `autoindex on;` in location; see dispatch branch above.
- How is traversal prevented? `isPathSafe(effectiveRoot, fullPath)` before serving/unlinking.
- CGI variables? See `startCgiDispatch` in `SocketManagerPost.cpp` and env assembly in `Cgi.cpp`.
- Chunked vs Content-Length? `tryParseHeaders` sets `st.isChunked`; `tryReadBody` chooses chunk decoder vs fixed length and enforces max body.
- Multipart saved where? `uploadDir` from route or default; files written to temp paths (see `MultipartStreamParser` callbacks in `SocketManager.cpp`).

## File Map
- Core loop: `srcs/server/SocketManager.cpp` (+ `SocketManagerHttp.cpp`, `SocketManagerPost.cpp`, `SocketManagerDelete.cpp`)
- CGI plumbing: `srcs/cgi/Cgi.cpp`
- Parsing: `srcs/cfg/ConfigLexer.cpp`, `ConfigParser.cpp`, `includes/Config.hpp`
- Utilities: `srcs/utils/utils.cpp`, `srcs/utils/file_utils.cpp`
- HTTP helpers: `srcs/server/Chunked.cpp`, `srcs/server/Response.cpp`
- Sockets: `srcs/server/ServerSocket.cpp`

Use this guide to locate functions quickly and explain the flow during an evaluation.
