#!/usr/bin/env python3
import os

# Mandatory HTTP header
print("Content-Type: text/plain")
print()  # blank line between headers and body

print("=== CGI environment variables ===")
for key, value in sorted(os.environ.items()):
    print("%s=%s" % (key, value))
