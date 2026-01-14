# Connect to server and wait for "Ready" message
# Ping sender

import socket

PORT = 5000
IP = '0.0.0.0'

def send_all(sock, data: bytes):
    total_sent = 0
    length = len(data)

    while total_sent < length:
        sent = sock.send(data[total_sent:])
        if sent <= 0:
            raise RuntimeError("Socket Failed")
        total_sent += sent

def recv_all(sock, length: int):
    pass

def send_framing():
    pass

def recv_framing():
    pass

def did_recv():
    pass


def main():
    serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    serversocket.bind((IP, PORT))
    serversocket.listen(5)


    print("Client A recieved 'READY' from server")

    for i in range(5):
        print(f"Client A sending PING {i+1}")


if __name__ == "__main__":
    main()