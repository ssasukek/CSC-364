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
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)((t.QuadPart * 1000000ULL) / (uint64_t)f.QuadPart);
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




    
    // printf("Echo server Listening on port %d\n", port);



    // printf("Round Trip Time (RTT) = min = %.2f ms, avg = %.2f ms, max = %.2f ms\n", min_rtt, avg_rtt, max_rtt);
    // printf("Throughput = %.2f Mbps\n", mbps);

    // cleanup - WSAcleanup();
    // closesocket(ls);
    // WSACleanup();
    // return 0;
}

// WSAGetLastError()


