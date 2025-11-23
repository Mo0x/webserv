#!/usr/bin/env python3
import sys

print("Status: 302 Found")
print("Location: /")
print("Content-Type: text/html")
print()
print("<html><body>")
print("<h1>Redirecting...</h1>")
print('<p>If you see this HTML, your client did not auto-follow the redirect.</p>')
print("</body></html>")
