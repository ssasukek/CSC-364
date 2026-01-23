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

struct BMPImage24
{
    int width = 0;
    int height = 0;
    int pre_height = 0;
    std::vector<uint8_t> rgb; // RGB data
};

static int row_padded(int width)
{
    // each row padding of 4 bytes
    return (width * 3 + 3) & (~3);
}

static BMPImage24 load_bmp(const char *filename)
{
    FILE *file = fopen(filename, "rb");

    BMPFileHeader fh;
    BMPInfoHeader ih;

    fread(&fh, sizeof(fh), 1, file);
    fread(&ih, sizeof(ih), 1, file);

    unsigned char *data = (unsigned char *)malloc(ih.biSizeImage);
    fseek(file, fh.bfOffBits, SEEK_SET);
    fread(data, ih.biSizeImage, 1, file);

    struct BMPImage24 img;
    img.width = ih.biWidth;
    img.height = (ih.biHeight > 0) ? ih.biHeight : -ih.biHeight; // handle top-down BMP
    img.pre_height = ih.biHeight;
    img.rgb.resize(row_padded(img.width) * img.height);
    std::vector<uint8_t> row((size_t)row_padded(img.width));

    if (fseek(file, fh.bfOffBits, SEEK_SET) != 0)
    {
        printf("Error seeking to pixel data");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < img.height; i++)
    {
        memcpy(row.data(), &data[i * row_padded(img.width)], row_padded(img.width));
        unsigned char *src_row = row.data();
        for (int j = 0; j < img.width; j++)
        {
            unsigned char b = src_row[j * 3 + 0];
            unsigned char g = src_row[j * 3 + 1];
            unsigned char r = src_row[j * 3 + 2];
            img.rgb[i * row_padded(img.width) + j * 3 + 0] = r;
            img.rgb[i * row_padded(img.width) + j * 3 + 1] = g;
            img.rgb[i * row_padded(img.width) + j * 3 + 2] = b;
        }
    }
    free(data);
    fclose(file);
    return img;
}

static void save_bmp(const char *filename, const struct BMPImage24 *img)
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
        memcpy(row, &img->rgb[i * row_padded], row_padded);
        fwrite(row, row_padded, 1, file);

        unsigned char *src_row = (unsigned char *)&img->rgb[i * row_padded];
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

static void free_image(struct BMPImage24 *img)
{
    img->rgb.clear();
    img->width = 0;
    img->height = 0;
    img->pre_height = 0;
}

// tone_mapping.cpp
static void tone_mapping(struct BMPImage24 *img, int use_sense);

static int parse_mode(int argc, char **argv)
{
    int mode = 0; // default sense reversing barrier
    if (argc >= 4)
    {
        if (strcmp(argv[3], "sense") == 0)
        {
            mode = 0;
        }
        else if (strcmp(argv[3], "diy") == 1)
        {
            mode = 1;
        }
    }
    return mode;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: %s <input.bmp> <output.bmp> [mode]\n", argv[0]);
    }

    int use_sense = parse_mode(argc, argv);

    BMPImage24 img = load_bmp(argv[1]);
    tone_mapping(&img, use_sense);
    save_bmp(argv[2], &img);
    free_image(&img);
    return 0;
}

#include "reversing_barrier.cpp"
#include "tone_mapping.cpp"