#!/usr/bin/python3
"""Synthetic EOF-sensitive legacy peer, isolated test use only."""
import os
from pathlib import Path
import socket
import struct
import sys
import time

args = sys.argv[1:]
name = args[args.index('--socket') + 1]
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(name)
kind = os.environ.get('PAIR_MODE', 'partial')
child = os.fork()
if child == 0:
    Path('/tmp/fixture/peer-child').touch()
    if not sock.recv(1):
        Path('/tmp/fixture/peer-saw-eof').touch()
    os._exit(0)
while not Path('/tmp/fixture/peer-child').exists():
    time.sleep(0.01)
if kind == 'oversized':
    sock.sendall(struct.pack('=q', 1024 * 1024 + 1))
elif kind == 'invalid-opcode':
    sock.sendall(struct.pack('=q', 12) + struct.pack('>iq', 999, 1))
else:
    sock.sendall(b'\x0c')  # A deliberately partial length prefix.
time.sleep(20)
