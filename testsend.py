import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(struct.pack("!9b", 15, 15, 0, 0, 0, 0, 0, 0, 0), ("192.168.1.65", 7765))
