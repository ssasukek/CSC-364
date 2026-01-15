
#include <iostream>
#include <fstream>
#define STB_IMAGE_IMPLEMENTATION
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
using namespace std;

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stop_token>
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

// sending 4 byte legnth framing
static bool send_framing(SOCKET s, char *buf, int len){
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

struct relay_args{
    SOCKET source_s;
    SOCKET dest_s;
    volatile LONG *stop_event;
    const char *direction;
};

// relay data concurrently in both directions
static DWORD WINAPI tcp_relay(LPVOID p){
    relay_args *args = (relay_args *)p;

    while (*(args->stop_event) == 0){
        std::string msg;

        if (!recv_framing(args->source_s, msg)){
            printf("Connection closed in direction %s\n", args->direction);
            *(args->stop_event) = 1;
            break;
        }

        if (!send_framing(args->dest_s, msg.data(), (int)msg.size())){
            printf("Failed to send in direction %s\n", args->direction);
            *(args->stop_event) = 1;
            break;
        }
    }

    // unblock other direction
    shutdown(args->dest_s, SD_SEND);
    return 0;

}

int main(int argc, char **argv)
{
    int port = atoi(argv[1]);
    WSADATA w;
    WSAStartup(MAKEWORD(2, 2), &w);

    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    int yes = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    bind(ls, (struct sockaddr *)&addr, sizeof(addr));
    listen(ls, 5);
    printf("Listening on port %d\n", port);

    for (;;)
    {
        printf("Waiting for connection A...\n");
        SOCKET a = accept(ls, NULL, NULL);
        if (a == INVALID_SOCKET)
            continue;
        printf("Connected to A\n");

        printf("Waiting for connection B...\n");
        SOCKET b = accept(ls, NULL, NULL);
        if (b == INVALID_SOCKET){
            closesocket(a);
            continue;
        }
        printf("Connected to B\n");

        // send READY to both clients
        const char *ready_msg = "READY\n";
        if(!send_all(a, ready_msg, (int)strlen(ready_msg)) || !send_all(b, ready_msg, (int)strlen(ready_msg))){
            printf("Failed to send READY\n");
            closesocket(a);
            closesocket(b);
            continue;
        }

        volatile LONG stop_event = 0;
        relay_args args1 = { a, b, &stop_event, "A->B" };
        relay_args args2 = { b, a, &stop_event, "B->A" };

        HANDLE t1 = CreateThread(NULL, 0, tcp_relay, &args1, 0, NULL);
        HANDLE t2 = CreateThread(NULL, 0, tcp_relay, &args2, 0, NULL);

        WaitForSingleObject(t1, INFINITE);
        WaitForSingleObject(t2, INFINITE);
        CloseHandle(t1);
        CloseHandle(t2);

        closesocket(a);
        closesocket(b);
    }

    closesocket(ls);
    WSACleanup();
    return 0;
}
