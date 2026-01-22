// due jan 23, 2024
// 4 threqads + two stage tone mapping + barrier + gather fucntion
// /program <input.bmp> <output.bmp>
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <iostream>
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

struct Image24
{
    int width = 0;
    int height = 0;
    int row_padded = (width * 3 + 3) & (~3);
    std::vector<uint8_t> data; // RGB data
};

struct Image24 load_bmp(const char *filename)
{
    FILE *file = fopen(filename, "rb");

    BMPFileHeader fh;
    BMPInfoHeader ih;

    fread(&fh, sizeof(BMPFileHeader), 1, file);
    fread(&ih, sizeof(BMPInfoHeader), 1, file);

    fread(&fh.bfType, sizeof(short), 1, file);
    fread(&fh.bfSize, sizeof(int), 1, file);
    fread(&fh.bfReserved1, sizeof(short), 1, file);
    fread(&fh.bfReserved2, sizeof(short), 1, file);
    fread(&fh.bfOffBits, sizeof(int), 1, file);


    unsigned char *data = (unsigned char *)malloc(ih.biSizeImage);
    fread(data, ih.biSizeImage, 1, file);

    struct Image24 img;
    img.width = ih.biWidth;
    img.height = ih.biHeight;
    img.data.assign(data, data + ih.biSizeImage);
    free(data);
    return img;
}

void save_bmp(const char *filename, const struct Image24 *img)
{
    FILE *file = fopen(filename, "wb");

    int width = img->width;
    int height = img->height;
    int row_padded = (width * 3 + 3) & (~3);
    std::vector<uint8_t> data; // RGB data

    struct BMPFileHeader fh;
    memset(&fh, 0, sizeof(fh));
    fh.bfType = 0x4D42;
    fh.bfSize = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + row_padded * height;
    fh.bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

    struct BMPInfoHeader ih;
    memset(&ih, 0, sizeof(ih));
    ih.biSize = sizeof(BMPInfoHeader);
    ih.biWidth = width;
    ih.biHeight = height;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = 0;
    ih.biSizeImage = row_padded * height;

    fwrite(&fh, sizeof(BMPFileHeader), 1, file);
    fwrite(&ih, sizeof(BMPInfoHeader), 1, file);

    unsigned char *row = (unsigned char *)malloc(row_padded);
    for (int i = 0; i < height; i++)
    {
        memcpy(row, &img->data[i * row_padded], row_padded);
        fwrite(row, row_padded, 1, file);

        unsigned char *src_row = (unsigned char *)&img->data[i * row_padded];
        memset(row, 0, row_padded);
        for (int j = 0; j < width; j++)
        {
            unsigned char r = src_row[j * 3 + 0];
            unsigned char g = src_row[j * 3 + 1];
            unsigned char b = src_row[j * 3 + 2];
            row[j * 3 + 0] = b;
            row[j * 3 + 1] = g;
            row[j * 3 + 2] = r;
        }
    }
    free(row);
    fclose(file);
}

void free_image(struct Image24 *img)
{
    img->data.clear();
    img->width = 0;
    img->height = 0;
}