#!/usr/bin/env python3
import sys, time

# Sleep without sending anything
time.sleep(5)

print("Content-Type: text/plain")
print()
print("Done after long sleep")
sys.stdout.flush()