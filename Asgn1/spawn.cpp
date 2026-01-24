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

// send data with 4-byte length
static bool send_data(SOCKET s, int32_t &in_msg)
{
    uint32_t net_len = 0;
    if (send_all(s, (char *)&net_len, sizeof(net_len)) > 0)
    {
        return false;
    }
    in_msg = (int32_t)ntohl(net_len);
    return true;
}

static int row_padded(int width)
{
    return (width * 3 + 3) & (~3);
}

struct BMPImage24
{
    int width = 0;
    int height = 0;
    int pre_height = 0;
    std::vector<uint8_t> bgr; // BGR data
};

static BMPImage24 load_bmp(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        printf("Error opening file: %s\n", filename);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Loading file: %s\n", filename);
    }

    BMPFileHeader fh;
    BMPInfoHeader ih;

    if (fread(&fh, sizeof(fh), 1, file) != 1 || fread(&ih, sizeof(ih), 1, file) != 1)
    {
        printf("Failed reading BMP header\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    BMPImage24 img;
    img.width = ih.biWidth;
    img.height = (ih.biHeight > 0) ? ih.biHeight : -ih.biHeight; // handle top-down BMP
    img.pre_height = ih.biHeight;

    int padding = row_padded(img.width);
    int image_bytes = padding * img.height;
    img.bgr.resize((size_t)image_bytes);

    if (fseek(file, fh.bfOffBits, SEEK_SET) != 0)
    {
        printf("Can't find pixel data\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    if (fread(img.bgr.data(), 1, image_bytes, file) != image_bytes)
    {
        printf("Can't read pixel data\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fclose(file);
    printf("BMP Image loaded: %dx%d\n", img.width, img.height);
    return img;
}

static void save_bmp(const char *filename, const BMPImage24 *img)
{
    FILE *file = fopen(filename, "wb");
    if (!file)
    {
        printf("Error opening file for writing: %s\n", filename);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Saving file: %s\n", filename);
    }

    int width = img->width;
    int height = img->height;
    int padding = row_padded(width);

    struct BMPFileHeader fh;
    memset(&fh, 0, sizeof(fh));
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
    fh.bfSize = fh.bfOffBits + (unsigned int)(padding * height);

    struct BMPInfoHeader ih;
    memset(&ih, 0, sizeof(ih));
    ih.biSize = sizeof(BMPInfoHeader);
    ih.biWidth = width;
    ih.biHeight = height;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = 0;
    ih.biSizeImage = (unsigned int)(padding * height);

    fwrite(&fh, sizeof(fh), 1, file);
    fwrite(&ih, sizeof(ih), 1, file);
    fwrite(img->bgr.data(), 1, img->bgr.size(), file);
    fclose(file);
}

static const char *IP = "127.0.0.1";
static const int PORT = 5000;

int main(int argc, char **argv)
{
    // ./spawn calc 10 input.bmp output.bmp
    if (argc != 5)
    {
        printf("Usage: ./spawn <worker_exe> <num_workers> <input.bmp> <output.bmp>\n");
        return 1;
    }

    const char *worker_path = argv[1];
    int num_workers = atoi(argv[2]);
    const char *input_bmp = argv[3];
    const char *output_bmp = argv[4];

    const char *server_ip = IP;
    int port = PORT;
    int contrast = 50; // constrast value - change as needed\

    BMPImage24 img = load_bmp(input_bmp);
    BMPImage24 out = img;
    out.bgr.assign(img.bgr.size(), 0);
    
    int padding = row_padded(img.width);

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

    // spawn worker processes
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    char cmd[512];
    for (int i = 0; i < num_workers; i++)
    {
        snprintf(cmd, sizeof(cmd), "\"%s\" %s %d %d", worker_path, server_ip, port, contrast);

        // Start the child process.
        if (!CreateProcess(
                NULL,  // No module name (use command line)
                cmd,   // Command line
                NULL,  // Process handle not inheritable
                NULL,  // Thread handle not inheritable
                FALSE, // Set handle inheritance to FALSE
                0,     // No creation flags
                NULL,  // Use parent's environment block
                NULL,  // Use parent's starting directory
                &si,   // Pointer to STARTUPINFO structure
                &pi)   // Pointer to PROCESS_INFORMATION structure
        )
        {
            printf("CreateProcess failed (%d).\n", GetLastError());
            return;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        closesocket(ls);
        WSACleanup();
        return 1;
    }

    vector<SOCKET> worker_sockets;
    worker_sockets.reserve((size_t)num_workers);

    for (int i = 0; i < num_workers; i++)
    {
        printf("Waiting for worker %d to connect...\n", i);
        SOCKET s = accept(ls, NULL, NULL);
        if (s == INVALID_SOCKET)
        {
            printf("Accept failed\n");
            continue;
        }
        printf("Worker %d connected\n", i);
        worker_sockets.push_back(s);
    }

    

}
