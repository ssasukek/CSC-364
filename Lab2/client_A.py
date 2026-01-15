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

# receive exact number of length bytes
def recv_length(sock: socket.socket, length: int):
    data = []
    while length > 0:
        chunk = sock.recv(length)
        if not chunk:
            raise EOFError("Socket disconnected")
        data.append(chunk)
        length -= len(chunk)
    return b''.join(data)

def send_framing(sock: socket.socket, data: bytes):
    header = struct.pack('!I', len(data)) # 4-byte length header
    send_all(sock, header + data)

def recv_framing(sock: socket.socket):    
    header = recv_length(sock, 4)
    length, = struct.unpack('!I', header)
    return recv_length(sock, length)

# check if we received READY message
def did_recv(sock: socket.socket):
    buf = b''

    while b'\n' not in buf:
        data = sock.recv(1024)
        if not data:
            raise EOFError("Socket disconnected")
        buf += data

    line = b'\n'
    if line + b'\n' == b'READY\n':
        return True
    return False

def main():
    serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    serversocket.connect((IP, PORT))
    did_recv(serversocket)
    print("Client A recieved 'READY' from server")

    for i in range(5):
        msg = f"PING {i + 1}".encode()
        print(f"Client A sending PING {msg.decode()}")
        send_framing(serversocket, msg)

        reply = recv_framing(serversocket)
        print(f"Client A recieved: {reply.decode()}")

if __name__ == "__main__":
    main()