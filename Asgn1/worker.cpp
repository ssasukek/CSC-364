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
    if (argc != 3)
    {
        printf("Usage: %s <server_ip> <server_port>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);

    // Server setup
    WSADATA w;
    WSAStartup(MAKEWORD(2, 2), &w);

    SOCKET ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    bind(ls, (struct sockaddr *)&addr, sizeof(addr));
    listen(ls, 5);
    printf("Listening on port %d\n", port);

    int32_t rows = 0, width = 0, contrast = 0, row_padded = 0;
    int bytes = rows * row_padded;
    vector<uint8_t> bgr((size_t)bytes);

    int pixel_bytes = width * 3;
    for (int r = 0; r < rows; r++)
    {
        uint8_t *row_pt = bgr.data() + (size_t)r * (size_t)row_padded;

        for (int j = 0; j < pixel_bytes; j++)
        {
            row_pt[j] = (uint8_t)adjust((int)row_pt[j], contrast);
        }
    }

    closesocket(ls);
    WSACleanup();
    return 0;
}
