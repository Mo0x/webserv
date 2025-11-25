#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/plain")
print()

print("Hello from CGI debug script!")
print("-----------------------------")
print("REQUEST_METHOD =", os.environ.get("REQUEST_METHOD"))
print("QUERY_STRING   =", os.environ.get("QUERY_STRING"))
print("CONTENT_TYPE   =", os.environ.get("CONTENT_TYPE"))
print("CONTENT_LENGTH =", os.environ.get("CONTENT_LENGTH"))

if os.environ.get("REQUEST_METHOD") == "POST":
    length = int(os.environ.get("CONTENT_LENGTH") or "0")
    body = sys.stdin.read(length)
    print()
    print("BODY START")
    print(body)
    print("BODY END")
