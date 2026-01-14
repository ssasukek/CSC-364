### TCP Relay Server - that allow two clients to communicate through it ###
# Bind > Listen > Accept

import socket
import threading

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


# def recv_all(sock: socket.socket, length: int):


def main():
    serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    serversocket.bind((IP, PORT))
    serversocket.listen(5)
    
    print(f"Relay server started on port: {PORT}")

    clientA, addrA = serversocket.accept()
    print(f"Client A connected from {addrA}")

    clientB, addrB = serversocket.accept()
    print(f"Client B connected from {addrB}")

    try:
        send_all(clientA, b"READY\n")
        send_all(clientB, b"READY\n")
    except Exception as e:
        clientA.close()
        clientB.close()
        return



if __name__ == "__main__":
    main()
