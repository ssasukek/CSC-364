#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#pragma comment(lib, "Ws2_32.lib")

#include <iostream>
#include <fstream>
#include <thread>
#include <iostream>

// measuring time in micro seconds
static uint64_t now_us(void){
    static LARGE_INTEGER f;
    static int init = 0;
    if (!init){
        QueryPerformanceFrequency(&f);
        init = 1;
    }
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return (uint64_t)((time.QuadPart * 1000000ULL) / (uint64_t)f.QuadPart);
}

// Connect to TCP
static SOCKET connect_tcp(const char *hostname, const char *port){
    struct addrinfo hints, *res = 0, *p = 0;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int info = getaddrinfo(hostname, port, &hints, &res);
    if (info != 0){
        printf("getaddrinfo failed: %d\n", info);
        return INVALID_SOCKET;
    }

    for (p = res; p; p = p->ai_next){
        SOCKET s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == INVALID_SOCKET){
            continue;
        }
        if (connect(s, p->ai_addr, (int)p->ai_addrlen) == 0){
            printf("Connected\n");
            freeaddrinfo(res);
            return s;
        }
        closesocket(s);
    }
    freeaddrinfo(res);
    return INVALID_SOCKET;
}

// Send all data in buffer
static int send_all(SOCKET s, const char *buf, int len){
    int total_sent = 0;
    while (total_sent < len){
        int n = send(s, buf + total_sent, len - total_sent, 0);
        if (n <= 0){
            return n;
        }
        total_sent += n;
    }
    return 1;
}

// Receive all data in buffer
static int recv_all(SOCKET s, char *buf, int len){
    int total_recv = 0;
    while (total_recv < len){
        int n = recv(s, buf + total_recv, len - total_recv, 0);
        if (n <= 0){
            return n;
        }
        total_recv += n;
    }
    return 1;
}

// [programname] [hostname] [port] [pings] [ping_bytes] [bulk_bytes]
// ./netprobe localhost 5000 100 32 1048576
int main(int argc, char **argv){

    if (argc < 6){
        printf("Usage: %s <hostname> <port> <pings> <ping_bytes> <bulk_bytes>\n", argv[0]);
        return 1;
    }

    const char *hostname = argv[1];
    const char *port_str = argv[2];
    int pings = atoi(argv[3]);
    int ping_bytes = atoi(argv[4]);
    int bulk_bytes = atoi(argv[5]);

    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0){
        printf("WSAStartup failed\n");
        return 1;
    }

    SOCKET socket = connect_tcp(hostname, port_str);
    if (socket == INVALID_SOCKET){
        printf("Failed to connect to %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // ping iterations - measure RTT
    int buffer[3200];
    uint64_t min_rtt = UINT64_MAX;
    uint64_t max_rtt = 0;
    uint64_t total_rtt = 0;

    for (int i = 0; i < pings; i++){
        uint64_t start = now_us();

        // send ping_bytes
        if (send_all(socket, (const char *)buffer, ping_bytes) <= 0){
            printf("send failed: %d\n", WSAGetLastError());
            closesocket(socket);
            WSACleanup();
            return 1;
        }

        // receive ping_bytes
        if (recv_all(socket, (char *)buffer, ping_bytes) <= 0){
            printf("recv failed: %d\n", WSAGetLastError());
            closesocket(socket);
            WSACleanup();
            return 1;
        }

        uint64_t rtt_us = now_us() - start;

        if (rtt_us < min_rtt) min_rtt = rtt_us;
        if (rtt_us > max_rtt) max_rtt = rtt_us;
        total_rtt += rtt_us;
    }
    double avg_rtt = (double)total_rtt / (double)pings;
    printf("Round Trip Time (RTT): min = %llu us, avg = %.2f us, max = %llu us\n", min_rtt, avg_rtt, max_rtt);

    // Bulk transfer - measure throughput
    uint64_t start = now_us();

    //sender (Writes a chunk of data (e.g., a byte[] or Buffer) to the socket.)
    int chunk_bytes = (int)sizeof(buffer);
    int bulk_remaining = bulk_bytes;
    while (bulk_remaining > 0){
        int to_send = (bulk_remaining < chunk_bytes) ? bulk_remaining : chunk_bytes;
        if (send_all(socket, (const char *)buffer, to_send) <= 0){
            closesocket(socket);
            WSACleanup();
            return 1;
        }

    //receiver (Reads data from the socket)
        int to_recv = (bulk_remaining < chunk_bytes) ? bulk_remaining : chunk_bytes;
        if (recv_all(socket, (char *)buffer, to_recv) <= 0){
            closesocket(socket);
            WSACleanup();
            return 1;
        }
        bulk_remaining -= to_recv;
    }

    uint64_t total_us = now_us() - start;

    // Calculate throughput in MB/s
    double seconds = (double)total_us / 1000000;    // Seconds = Microseconds / 1,000,000
    double total_bytes = (double)bulk_bytes * 2;
    double MBps = (total_bytes / seconds) / 1000000;    

    printf("Throughput = %.2f MB/s\n", MBps);


    closesocket(socket);
    WSACleanup();
    return 0;
}


