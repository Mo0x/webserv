#!/usr/bin/env python3
import os
import sys

body_lines = [
    "Hello from CGI debug script!",
    "-----------------------------",
    "REQUEST_METHOD = " + str(os.environ.get("REQUEST_METHOD")),
    "QUERY_STRING   = " + str(os.environ.get("QUERY_STRING")),
    "CONTENT_TYPE   = " + str(os.environ.get("CONTENT_TYPE")),
    "CONTENT_LENGTH = " + str(os.environ.get("CONTENT_LENGTH")),
]
body = "\n".join(body_lines) + "\n"

print("Content-Type: text/plain")
print(f"Content-Length: {len(body)}")
print()
print(body, end="")
