// [programname] [hostname] [port] [pings] [ping_bytes] [bulk_bytes]

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
    return (uint16_t)((t.QuadPart * 1000000ULL) / (uint64_t)f.QuadPart);
}

// Connect to TCP
static SOCKET connect_tcp(const char *hostname, const char *port){
    struct addrinfo hints, *res = 0, *p = 0;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(hostname, port, &hints, &res) != 0){
        return INVALID_SOCKET;
    }

    for (p = res; p; p = p->ai_next){
        SOCKET s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == INVALID_SOCKET){
            continue;
        }
        if (connect(s, p->ai_addr, (int)p->ai_addrlen) == 0){
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


int main(int argc, char **argv){

    if (argc < 3){
        printf("Usage: %s <hostname> <port> <pings>", argv[0]);
        return 1;
    }

    int port = atoi(argv[2]);
    WSADATA w;
    WSAStartup(MAKEWORD(2,2), &w);
    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr = { 0 };

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    bind(ls, (struct sockaddr*)&addr, sizeof(addr));
    listen(ls, 5);
    
    printf("Echo server Listening on port %d\n", port);

    for (;;){
        SOCKET c = accept(ls, 0, 0);
        if (c == INVALID_SOCKET){
            continue;
        }
        char buf[4096];
        int n;
        while ((n = recv(c, buf, sizeof(buf), 0)) > 0){
            send(c, buf, n, 0);
        }
        closesocket(c);
    }




    printf("Round Trip Time (RTT) = min = %.2f ms, avg = %.2f ms, max = %.2f ms\n", min_rtt, avg_rtt, max_rtt);
    printf("Throughput = %.2f Mbps\n", mbps);

    // cleanup - WSAcleanup();
    closesocket(ls);
    WSACleanup();
    return 0;
}

// WSAGetLastError()


