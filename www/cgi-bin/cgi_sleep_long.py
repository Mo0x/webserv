#!/usr/bin/env python3
import sys, time

print("Content-Type: text/plain")
print()
print("Starting long CGI...")
sys.stdout.flush()

# Sleep longer than cgi_timeout_ms (2 seconds)
time.sleep(5)

print("Finished after sleep (you should NOT see this if timeout works)")
