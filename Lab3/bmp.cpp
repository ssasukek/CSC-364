// due jan 23, 2024
// 4 threqads + two stage tone mapping + barrier + gather fucntion
// /program <input.bmp> <output.bmp>

#include <iostream>
#include <cstdint>
#include <vector>

#pragma pack(push, 1)
struct BMPFileHeader{
    unsigned short bfType;      // 'BM' = 0x4D42
    unsigned int bfSize;        // file size in bytes
    unsigned short bfReserved1; // must be 0
    unsigned short bfReserved2; // must be 0
    unsigned int bfOffBits;     // offset to pixel data
};
struct BMPInfoHeader{
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

struct Pixel_RGB{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct Image{
    int width = 0;
    int height = 0;
    std::vector<Pixel_RGB> pixels;
};

struct BMP{
    BMPFileHeader file_header;
    BMPInfoHeader info_header;
};

int main(){
    FILE *file = fopen("test.bmp", "rb");
    BMPFileHeader fh;
    BMPInfoHeader ih;
    fread(&fh, sizeof(BMPFileHeader), 1, file);
    fread(&ih, sizeof(BMPInfoHeader), 1, file);

    unsigned char *data = (unsigned char *)malloc(ih.biSizeImage);

    BMP bmp;
    std::cout << "Size of BMP struct: " << sizeof(bmp) << " bytes\n";
    return 0;
}