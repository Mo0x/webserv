#!/usr/bin/env python3
"""
Ad-hoc multipart regression tests.

Scenarios covered:
1. Happy path upload with a field and a small file.
2. Missing multipart boundary header → 400.
3. Oversized file → 413.
4. Truncated body (no closing boundary) → 400.
5. Mixed parts to verify field exposure in the response summary.

These tests expect the server config at config.conf with an /upload route
that allows POST + multipart/form-data as defined in the subject.
"""

import os
import socket
import subprocess
import sys
import time
from http.client import HTTPConnection


REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER_BIN = os.path.join(REPO_ROOT, "webserv")
CONFIG = os.path.join(REPO_ROOT, "config.conf")
HOST = "127.0.0.1"
PORT = 8081


def build_multipart(parts, boundary):
    """parts: list of (headers_dict, body_string)"""
    lines = []
    for headers, body in parts:
        lines.append(f"--{boundary}\r\n")
        for name, value in headers.items():
            lines.append(f"{name}: {value}\r\n")
        lines.append("\r\n")
        lines.append(body)
        if not body.endswith("\r\n"):
            lines.append("\r\n")
    lines.append(f"--{boundary}--\r\n")
    payload = "".join(lines)
    return payload.encode("utf-8")


def send_request(body_bytes, headers):
    conn = HTTPConnection(HOST, PORT, timeout=3)
    conn.request("POST", "/upload", body=body_bytes, headers=headers)
    resp = conn.getresponse()
    data = resp.read().decode("utf-8", "replace")
    conn.close()
    return resp.status, data


def happy_path():
    boundary = "----BOUNDARY_HAPPY"
    parts = [
        (
            {"Content-Disposition": 'form-data; name="note"'},
            "hello world\r\n",
        ),
        (
            {
                "Content-Disposition": 'form-data; name="file"; filename="demo.txt"',
                "Content-Type": "text/plain",
            },
            "demo body\r\n",
        ),
    ]
    payload = build_multipart(parts, boundary)
    status, body = send_request(
        payload,
        {
            "Content-Type": f"multipart/form-data; boundary={boundary}",
            "Content-Length": str(len(payload)),
        },
    )
    assert status == 201, f"expected 201, got {status}"
    assert "Uploaded files:" in body, "missing upload summary"
    assert "note: hello world" in body, "field not echoed in summary"
    print("✔ happy path")


def missing_boundary():
    payload = b"--oops\r\n\r\n"
    status, _ = send_request(
        payload,
        {
            "Content-Type": "multipart/form-data",  # no boundary parameter
            "Content-Length": str(len(payload)),
        },
    )
    assert status == 400, f"expected 400 for missing boundary, got {status}"
    print("✔ missing boundary -> 400")


def oversized_file():
    boundary = "----BOUNDARY_BIG"
    blob = "A" * 1_048_577  # just over the 1 MiB limit in config.conf
    parts = [
        (
            {
                "Content-Disposition": 'form-data; name="file"; filename="big.bin"',
                "Content-Type": "application/octet-stream",
            },
            blob + "\r\n",
        )
    ]
    payload = build_multipart(parts, boundary)
    status, _ = send_request(
        payload,
        {
            "Content-Type": f"multipart/form-data; boundary={boundary}",
            "Content-Length": str(len(payload)),
        },
    )
    assert status == 413, f"expected 413 for oversized upload, got {status}"
    print("✔ oversized body -> 413")


def truncated_body():
    boundary = "----BOUNDARY_TRUNC"
    headers = [
        "POST /upload HTTP/1.1",
        f"Host: {HOST}:{PORT}",
        f"Content-Type: multipart/form-data; boundary={boundary}",
    ]
    parts = [
        f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; filename=\"foo.txt\"\r\n\r\n123\r\n",
        f"--{boundary}\r\nContent-Disposition: form-data; name=\"note\"\r\n\r\npartial",
    ]
    body = "".join(parts)
    headers.append(f"Content-Length: {len(body)}")
    request = "\r\n".join(headers) + "\r\n\r\n" + body
    with socket.create_connection((HOST, PORT), timeout=3) as sock:
        sock.sendall(request.encode("utf-8"))
        sock.shutdown(socket.SHUT_WR)  # close early; no closing boundary
        response = sock.recv(4096).decode("utf-8", "replace")
    assert "400" in response, "expected 400 response for truncated body"
    print("✔ truncated body -> 400")


def mixed_parts():
    boundary = "----BOUNDARY_FIELDS"
    parts = [
        ({"Content-Disposition": 'form-data; name="alpha"'}, "one\r\n"),
        (
            {
                "Content-Disposition": 'form-data; name="file"; filename="field.txt"',
                "Content-Type": "text/plain",
            },
            "payload\r\n",
        ),
        ({"Content-Disposition": 'form-data; name="beta"'}, "two\r\n"),
    ]
    payload = build_multipart(parts, boundary)
    status, body = send_request(
        payload,
        {
            "Content-Type": f"multipart/form-data; boundary={boundary}",
            "Content-Length": str(len(payload)),
        },
    )
    assert status == 201, f"expected 201, got {status}"
    assert "alpha: one" in body and "beta: two" in body, "fields missing in summary"
    print("✔ mixed parts -> fields echoed")


def wait_for_server(timeout=3.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection((HOST, PORT), timeout=1):
                pass
            return True
        except Exception:
            time.sleep(0.1)
    return False


def main():
    if not os.path.exists(SERVER_BIN):
        print("webserv binary not found; build the project first.", file=sys.stderr)
        return 1

    server = subprocess.Popen(
        [SERVER_BIN, CONFIG],
        cwd=REPO_ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if not wait_for_server():
        print("Server did not start listening in time.", file=sys.stderr)
        server.terminate()
        return 1
    time.sleep(0.2)
    try:
        happy_path()
        missing_boundary()
        oversized_file()
        truncated_body()
        mixed_parts()
    except AssertionError as exc:
        print(f"FAILED: {exc}", file=sys.stderr)
        return 1
    finally:
        server.terminate()
        try:
            server.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server.kill()
    return 0


if __name__ == "__main__":
    sys.exit(main())
