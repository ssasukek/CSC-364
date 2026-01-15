### TCP Relay Server - that allow two clients to communicate through it ###
# Bind > Listen > Accept

import socket
import struct
import threading

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

# sending 4 byte legnth framing
def send_framing(sock: socket.socket, data: bytes):
    header = struct.pack('!I', len(data)) # 4-byte length header
    send_all(sock, header + data)

# receive framed message
def recv_framing(sock: socket.socket):    
    header = recv_length(sock, 4)
    length, = struct.unpack('!I', header)
    return recv_length(sock, length)

# relay data concurrently in both directions
def tcp_relay(source_s: socket.socket, dest_s: socket.socket, stop_event: threading.Event, direction: str):
    try:
        while not stop_event.is_set():
            data = recv_framing(source_s)
            print(f"Relaying {len(data)} bytes {direction}")
            send_framing(dest_s, data)
    except Exception as e:
        stop_event.set()
        print(f"Stopping relay {direction}: {e}")
    finally:
        try:
            dest_s.shutdown(socket.SHUT_WR)
        except Exception:
            pass

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
    
    stop = threading.Event()

    t1 = threading.Thread(target=tcp_relay, args=(clientA, clientB, stop, "A -> B"), daemon=True)
    t2 = threading.Thread(target=tcp_relay, args=(clientB, clientA, stop, "B -> A"), daemon=True)

    t1.start()
    t2.start()

    t1.join()
    t2.join()

    clientA.close()
    clientB.close()



if __name__ == "__main__":
    main()
