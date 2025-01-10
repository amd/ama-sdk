#!/usr/bin/python3
import zmq
import sys

context = zmq.Context()
Url=sys.argv[1]
socket = context.socket(zmq.REQ)
socket.connect(Url)
cmd = sys.stdin.read()
socket.send_string(cmd)
recv = socket.recv()
print(f"Received reply [{recv.decode()}]")
