#!/usr/bin/env python3
"""
Simple functional test for DELETE handling.

Starts the server with a minimal config on port 18081, creates a file under `www/`,
issues an HTTP DELETE with curl, and asserts the server returns 204 and removes the file.

This test uses only the Python standard library so it can run on most systems.
"""
import os
import sys
import time
import subprocess
import socket

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
WEBSERV = os.path.join(ROOT, 'webserv')
CONFIG = os.path.join(os.path.dirname(__file__), 'test_config.conf')
PORT = 18081

def wait_for_port(host, port, timeout=5.0):
    end = time.time() + timeout
    while time.time() < end:
        try:
            s = socket.create_connection((host, port), 0.5)
            s.close()
            return True
        except Exception:
            time.sleep(0.1)
    return False

def run():
    if not os.path.exists(WEBSERV):
        print('Error: compiled binary ./webserv not found. Run `make` first.', file=sys.stderr)
        return 2

    # ensure www dir exists
    www_dir = os.path.join(ROOT, 'www')
    if not os.path.isdir(www_dir):
        print('Error: www directory not found', file=sys.stderr)
        return 2

    # write test file
    target = os.path.join(www_dir, 'test_delete_target.txt')
    with open(target, 'w') as f:
        f.write('delete-me')

    # start server
    proc = subprocess.Popen([WEBSERV, CONFIG], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        started = wait_for_port('127.0.0.1', PORT, timeout=5.0)
        if not started:
            out, err = proc.communicate(timeout=1)
            print('Server failed to start. stdout:\n', out.decode(errors='ignore'))
            print('stderr:\n', err.decode(errors='ignore'))
            proc.kill()
            return 2

        # issue DELETE request using curl to keep it simple and portable
        url = 'http://127.0.0.1:%d/test_delete_target.txt' % PORT
        curl = subprocess.Popen(['curl', '-i', '-X', 'DELETE', url], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, err = curl.communicate(timeout=5)
        out = out.decode(errors='ignore')
        print('----- DELETE response -----')
        print(out)

        # check status code
        if '204' not in out.splitlines()[0]:
            print('Expected 204 No Content, got:', out.splitlines()[0])
            return 1

        if os.path.exists(target):
            print('File still exists after DELETE', file=sys.stderr)
            return 1

        # Try deleting again to get 404
        curl2 = subprocess.Popen(['curl', '-i', '-X', 'DELETE', url], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out2, err2 = curl2.communicate(timeout=5)
        out2 = out2.decode(errors='ignore')
        print('----- DELETE non-existent response -----')
        print(out2)
        if '404' not in out2.splitlines()[0]:
            print('Expected 404 Not Found on second delete, got:', out2.splitlines()[0])
            return 1

        print('DELETE tests passed')
        return 0
    finally:
        try:
            proc.terminate()
        except Exception:
            pass

if __name__ == '__main__':
    sys.exit(run())
