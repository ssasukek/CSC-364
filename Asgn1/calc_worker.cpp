#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <string>
#include <vector>
#include <iostream>
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

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

// receive data with 4-byte length
static bool recv_data(SOCKET s, int32_t &out_msg){
    uint32_t net_len = 0;
    if (recv_all(s, (char *)&net_len, sizeof(net_len)) <= 0){
        return false;
    }
    out_msg = (int32_t)ntohl(net_len);
    return true;
}

// send data with 4-byte length
static bool send_data(SOCKET s, int32_t &in_msg){
    uint32_t net_len = 0;
    if (send_all(s, (char *)&net_len, sizeof(net_len)) > 0){
        return false;
    }
    in_msg = (int32_t)ntohl(net_len);
    return true;
}

// contrast
int adjust(int x, int c){
    double f = (259.0 * (c + 255.0)) / (255.0 * (259.0 - c));
    int y = (int)(f * (x - 128) + 128);
    if (y < 0)
        y = 0;
    if (y > 255)
        y = 255;
    return y;
}

// Connect to TCP
static SOCKET connect_tcp(const char *server_ip, const char *port){
    // connect to server
    struct addrinfo hints, *res = 0, *p = 0;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int info = getaddrinfo(server_ip, port, &hints, &res);
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

int main(int argc, char **argv){
    if (argc != 4){
        printf("Usage: calc_worker <server_ip> <server_port> <contrast_value>\n");
        return 1;
    }
    const char *server_ip = argv[1];
    const char *server_port = argv[2];
    int contrast = atoi(argv[3]);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0){
        printf("WSAStartup failed\n");
        return 1;
    }
    SOCKET s = connect_tcp(server_ip, server_port);
    if (s == INVALID_SOCKET){
        printf("Failed to connect to server\n");
        WSACleanup();
        return 1;
    }

    // math - calculatation worker
    int32_t rows = 0, width = 0;
    if (!recv_data(s, rows) || !recv_data(s, width)){
        printf("Failed to receive image size\n");
        closesocket(s);
        WSACleanup();
        return 1;
    }
    for (int i = 0; i < rows; i++){
        vector<uint8_t> row_data(width * 3);
        if (recv_all(s, (char *)row_data.data(), width * 3) <= 0){
            printf("Failed to receive row data\n");
            closesocket(s);
            WSACleanup();
            return 1;
        }
        // adjust contrast
        for (int j = 0; j < width * 3; j++){
            row_data[j] = (uint8_t)adjust(row_data[j], contrast);
        }
        // send back processed row
        if (send_all(s, (char *)row_data.data(), width * 3) <= 0){
            printf("Failed to send processed row data\n");
            closesocket(s);
            WSACleanup();
            return 1;
        }
    }
    closesocket(s);
    WSACleanup();
    return 0;
}
