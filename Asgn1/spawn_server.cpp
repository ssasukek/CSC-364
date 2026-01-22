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

#pragma pack(push, 1)
struct BMPFileHeader
{
    unsigned short bfType;      // 'BM' = 0x4D42
    unsigned int bfSize;        // file size in bytes
    unsigned short bfReserved1; // must be 0
    unsigned short bfReserved2; // must be 0
    unsigned int bfOffBits;     // offset to pixel data
};
struct BMPInfoHeader
{
    unsigned int biSize;         // header size (40)
    int biWidth;                 // image width
    int biHeight;                // image height
    unsigned short biPlanes;     // must be 1
    unsigned short biBitCount;   // 24 for RGB
    unsigned int biCompression;  // 0 = BI_RGB
    unsigned int biSizeImage;    // image data size (can be 0 for BI_RGB)
    int biXPelsPerMeter;         // resolution
    int biYPelsPerMeter;         // resolution
    unsigned int biClrUsed;      // colors used (0)
    unsigned int biClrImportant; // important colors (0)
};
#pragma pack(pop)

// read BMP image
static bool readImage(const char *filename, char **pixels, int32_t *width, int32_t *height, int32_t *size)
{
    FILE *imageFile = fopen(filename, "rb");
    BMPFileHeader fh;
    BMPInfoHeader fih;
    fread(&fh.bfType, sizeof(BMPFileHeader), 1, imageFile);
    fread(&fh.bfSize, sizeof(BMPInfoHeader), 1, imageFile);
    
}

// write BMP image
static bool writeImage(const char *filename, const char *data, int size)
{
    FILE *file = fopen(filename, "wb");
    if (!file)
    {
        return false;
    }
    fwrite(data, 1, size, file);
    fclose(file);
    return true;
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

// send data with 4-byte length
static bool send_data(SOCKET s, int32_t &in_msg){
    uint32_t net_len = 0;
    if (send_all(s, (char *)&net_len, sizeof(net_len)) > 0){
        return false;
    }
    in_msg = (int32_t)ntohl(net_len);
    return true;
}


int main(int argc, char **argv){

}
