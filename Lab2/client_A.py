# Connect to server and wait for "Ready" message
# Ping sender

import socket
import struct

PORT = 5000
IP = '0.0.0.0'

def send_all(sock: socket.socket, data: bytes):
    total_sent = 0
    length = len(data)

    while total_sent < length:
        sent = sock.send(data[total_sent:])
        if sent <= 0:
            raise RuntimeError("Socket Failed")
        total_sent += sent

def recv_all(sock: socket.socket, length: int):
    pass

def send_framing(sock: socket.socket, data: bytes):
    header = struct.pack('!I', len(data)) # 4-byte length header
    send_all(sock, header + data)

def recv_framing(sock: socket.socket):
    header = recv_all(sock, 4)
    return recv_all(sock, length)

# check if we received READY message
def did_recv(sock: socket.socket):
    pass


def main():
    serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)


    print("Client A recieved 'READY' from server")

    for i in range(5):
        print(f"Client A sending PING {i+1}")


if __name__ == "__main__":
    main()