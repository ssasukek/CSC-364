#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#pragma comment(lib, "Ws2_32.lib")

static const char *IP = "127.0.0.1";
static const int PORT = 5000;

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

// sending 4 byte legnth framing
static bool send_framing(SOCKET s, const char *buf, int len){
    uint32_t net_len = htonl(len);
    if (send_all(s, (char *)&net_len, sizeof(net_len)) <= 0){   // 4 bytes
        return false;
    }
    if (len == 0){
        return true;
    }
    return send_all(s, buf, len) > 0;
}

// receive framed message
static bool recv_framing(SOCKET s, std::string &out_msg){
    uint32_t net_len = 0;
    if (recv_all(s, (char *)&net_len, sizeof(net_len)) <= 0){
        return false;
    }
    uint32_t len = ntohl(net_len);
    out_msg.resize(len);
    if (len == 0){
        return true;
    }
    return recv_all(s, out_msg.data(), len) > 0;
}

static bool did_recv(SOCKET s){
    std::string msg;
    char c;
    while (true){
        int n = recv(s, &c, 1, 0);
        if (n <= 0){
            return false;
        }
        if (c == '\n'){
            break;
        }
        msg.push_back(c);
    }
    return msg == "READY";
}

int main(){
    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0){
        printf("WSAStartup failed\n");
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET){
        printf("Failed to connect to %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    
    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, IP, &server_addr.sin_addr);

    if (connect(sock, (sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR){
        printf("Failed to connect to server: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // debugging 
    if (!did_recv(sock)){
        printf("Did not receive READY from server\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // send ping message framed
    while (true){
        std::string ping_msg;
        if (!recv_framing(sock, ping_msg)){
            printf("Failed to receive ping message\n");
            break;
        }
        // printf("%s\n", ping_msg.c_str());

        Sleep(2000);

        int n = -1;
        sscanf(ping_msg.c_str(), "PING %d", &n);
        std::string reply_pong = "PONG " + std::to_string(n);
        printf("%s\n", reply_pong.c_str());

        if(!send_framing(sock, (char *)reply_pong.data(), (int)reply_pong.size())){
            printf("Failed to send PONG message\n");
            break;
        }
    }

    shutdown(sock, SD_BOTH);
    closesocket(sock);
    WSACleanup();
    return 0;
}