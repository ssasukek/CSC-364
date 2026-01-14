# Connect to server and wait for "Ready" message

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

def main():

    serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    serversocket.bind((IP, PORT))
    serversocket.listen(5)

    print("Client B recieved 'READY' from server")
    


if __name__ == "__main__":
    main()