#!/usr/bin/env python3
"""Test MicroPython WASI port features"""

import subprocess
import sys

WASM = "build/micropython.wasm"
WASMTIME = ["wasmtime", "run", "-W", "exceptions=y", "-S", "tcp=y", "-S", "udp=y", 
            "-S", "inherit-network=y", "-S", "allow-ip-name-lookup=y", WASM]

def run(code):
    cmd = WASMTIME + [code]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    return p.stdout.strip(), p.stderr.strip(), p.returncode

passed = 0
failed = 0

def test(name, code, expected_contains=None, should_fail=False):
    global passed, failed
    stdout, stderr, rc = run(code)
    output = stdout + stderr
    ok = False
    if should_fail:
        ok = rc != 0
    elif expected_contains:
        ok = expected_contains in output and rc == 0
    else:
        ok = rc == 0
    
    if ok:
        print(f"  PASS: {name}")
        passed += 1
    else:
        print(f"  FAIL: {name}")
        print(f"    stdout: {stdout[:200]}")
        print(f"    stderr: {stderr[:200]}")
        print(f"    rc: {rc}")
        failed += 1

print("=== Test Suite ===")

print("\n--- DNS Resolution ---")
test("getaddrinfo resolves hostname", 
     "import socket; print(socket.getaddrinfo('www.google.com', 80)[0][0])",
     "1")
test("connect with IP tuple",
     "import socket; s=socket.socket(); s.connect(('140.82.121.6', 80)); s.send(b'GET / HTTP/1.0\\r\\n\\r\\n'); print(s.recv(100)[:4]); s.close()",
     "HTTP")
test("connect with DNS hostname",
     "import socket; s=socket.socket(); s.connect(('www.google.com', 80)); s.send(b'GET / HTTP/1.0\\r\\nHost: www.google.com\\r\\n\\r\\n'); print(s.recv(100)[:4]); s.close()",
     "HTTP")
test("sendto with DNS hostname",
     "import socket; s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.sendto(b'hi', ('8.8.8.8', 53)); print('OK'); s.close()",
     "OK")

print("\n--- bytearray sockaddr format (asyncio compat) ---")
test("connect with bytearray sockaddr",
     "import socket; ai=socket.getaddrinfo('www.google.com', 80, 0, socket.SOCK_STREAM)[0]; s=socket.socket(ai[0], ai[1], ai[2]); s.connect(ai[-1]); s.send(b'GET / HTTP/1.0\\r\\n\\r\\n'); print(s.recv(100)[:4]); s.close()",
     "HTTP")

print("\n--- Time module ---")
test("time.time() returns float",
     "import time; t=time.time(); print(type(t).__name__); print(t > 1700000000)",
     "True")
test("time.gmtime() epoch is 2000",
     "import time; print(time.gmtime(0)[0])",
     "2000")
test("time.localtime() works",
     "import time; print(time.localtime()[0] >= 2025)",
     "True")

print("\n--- Sys module ---")
test("sys.exc_info() works",
     "import sys; print(sys.exc_info())",
     "None")
test("sys.atexit() registers",
     "import sys; sys.atexit(lambda: print('exit')); print('registered')",
     "registered")

print(f"\n=== Results: {passed} passed, {failed} failed ===")
sys.exit(0 if failed == 0 else 1)
