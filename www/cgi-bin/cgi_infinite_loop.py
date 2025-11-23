#!/usr/bin/env python3
import time

# Misbehaving CGI: no headers, no body, just hangs forever
while True:
    time.sleep(1)  # sleep to avoid 100% CPU
