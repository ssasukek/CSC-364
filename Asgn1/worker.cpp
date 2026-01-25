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
static int send_all(SOCKET s, const char *buf, int len)
{
    int total_sent = 0;
    while (total_sent < len)
    {
        int n = send(s, buf + total_sent, len - total_sent, 0);
        if (n <= 0)
        {
            return n;
        }
        total_sent += n;
    }
    return 1;
}

// Receive all data in buffer
static int recv_all(SOCKET s, char *buf, int len)
{
    int total_recv = 0;
    while (total_recv < len)
    {
        int n = recv(s, buf + total_recv, len - total_recv, 0);
        if (n <= 0)
        {
            return n;
        }
        total_recv += n;
    }
    return 1;
}

// receive data with 4-byte length
static bool recv_data(SOCKET s, int32_t &out_msg)
{
    uint32_t net_len = 0;
    if (recv_all(s, (char *)&net_len, sizeof(net_len)) <= 0)
    {
        return false;
    }
    out_msg = (int32_t)ntohl(net_len);
    return true;
}

static int row_padded(int width)
{
    return (width * 3 + 3) & (~3);
}

// contrast
int adjust(int x, int c)
{
    double f = (259.0 * (c + 255.0)) / (255.0 * (259.0 - c));
    int y = (int)(f * (x - 128) + 128);
    if (y < 0)
        y = 0;
    if (y > 255)
        y = 255;
    return y;
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        printf("Usage: %s <server_ip> <port> <contrast> [id]\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    int contrast = atoi(argv[3]);
    int id = (argc >= 5) ? atoi(argv[4]) : -1;

    // Server setup
    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w)){
        printf("WSAstartup failed");
        return 1;
    }

    SOCKET s= socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s== INVALID_SOCKET){
        printf("Socket failed");
        WSACleanup();
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);


    int32_t rows = 0;
    int32_t width = 0;

    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) != 1)
    {
        printf("id=%d bad IPv4: %s\n", id, server_ip);
        closesocket(s);
        WSACleanup();
        return 1;
    }

    if (connect(s, (sockaddr *)&addr, sizeof(addr)) != 0)
    {
        printf("id=%d connect failed\n", id);
        closesocket(s);
        WSACleanup();
        return 1;
    }

    if (!recv_data(s, rows) || !recv_data(s, width))
    {
        printf("id=%d recv header failed\n", id);
        closesocket(s);
        WSACleanup();
        return 1;
    }
    
    int padded = row_padded((int)width);
    int bytes = (int)rows * padded;
    std::vector<uint8_t> bgr((size_t)bytes);

    if (bytes > 0 && recv_all(s, (char *)bgr.data(), bytes) <= 0)
    {
        printf("id=%d recv chunk failed\n", id);
        closesocket(s);
        WSACleanup();
        return 1;
    }

    int pixel_bytes = (int)width * 3;
    for (int r = 0; r < rows; r++)
    {
        uint8_t *rowp = bgr.data() + (size_t)r * (size_t)padded;
        for (int j = 0; j < pixel_bytes; j++)
        {
            rowp[j] = (uint8_t)adjust((int)rowp[j], contrast);
        }
    }

    if (bytes > 0 && send_all(s, (const char *)bgr.data(), bytes) <= 0)
    {
        printf("id=%d send result failed\n", id);
        closesocket(s);
        WSACleanup();
        return 1;
    }

    closesocket(s);
    WSACleanup();
    return 0;
}
