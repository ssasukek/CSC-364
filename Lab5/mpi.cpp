// ./MPI worker [num of proc]

#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <iostream>
#include <atomic>
#include <stdatomic.h>
#include <thread>
#include <math.h>
#include <mpi.h> //mpiexec

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
    if (fh.bfType != 0x4D42 || ih.biBitCount != 24 || ih.biCompression != 0)
    {
        printf("not a 24 bit BMP\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    BMPImage24 img;
    img.width = ih.biWidth;
    img.height = (ih.biHeight > 0) ? ih.biHeight : -ih.biHeight;
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
    ih.biHeight = img->pre_height;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = 0;
    ih.biSizeImage = (unsigned int)(padding * height);

    fwrite(&fh, sizeof(fh), 1, file);
    fwrite(&ih, sizeof(ih), 1, file);
    fwrite(img->bgr.data(), 1, img->bgr.size(), file);
    fclose(file);
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // rank == id
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    // const int N = 10000;
    // int *arr = 0;

    if (argc < 3)
    {
        printf("Usage: mpi exec -n <n> ./mpi.exe worker <input.bmp> <output.bmp>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    const char *input_bmp = argv[1];
    const char *output_bmp = argv[2];

    BMPImage24 img = load_bmp(input_bmp);
    int padding = row_padded(img.width);

    MPI_Bcast(&img.width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&img.height, 1, MPI_INT, 0, MPI_COMM_WORLD);

    const int N = img.height;
    int base = N / nprocs, rem = N % nprocs;
    int nloc = base + (rank < rem);

    int *counts = 0, *displs = 0;
    if (rank == 0)
    {
        // arr = (int *)malloc(N * sizeof(int));
        // srand(1);
        // for (int i = 0; i < N; i++)
        //     arr[i] = rand() % 1000;

        counts = (int *)malloc(nprocs * sizeof(int));
        displs = (int *)malloc(nprocs * sizeof(int));

        int off = 0;
        for (int p = 0; p < nprocs; p++)
        {
            counts[p] = (base + (p < rem)) * padding;
            displs[p] = off * padding;
            off += base + (p < rem);
        }
    }

    // allocate mem for loc chunk
    unsigned char *loc = (unsigned char *)malloc(nloc * padding);

    MPI_Scatterv(img.bgr.data(), counts, displs, MPI_UNSIGNED_CHAR, loc, nloc * padding, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    // for (int i = 0; i < nloc; i++)
    //     loc[i]++;

    // per-entry arithmetic op

    MPI_Barrier(MPI_COMM_WORLD); // barrier between phases

    // avg illuminance
    long long lsum = 0;
    for (int i = 0; i < nloc; i++){
        for (int x = 0; x < img.width; x++){
            int index = (i * padding) + (x * 3);
            unsigned char B = loc[index];
            unsigned char G = loc[index + 1];
            unsigned char R = loc[index + 2];
            lsum += (B + G + R);
        }
    }

    long long gsum = 0;
    MPI_Reduce(&lsum, &gsum, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    double avg = 0.0;
    if (rank == 0){
        avg = gsum / (img.width * img.height * 3);
        printf("global sum: %lld, avg: %f\n", gsum, avg);
        avg = avg / 128.0; 
    }

    MPI_Bcast(&avg, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // dot product
    // (B,G,R) *= (0.9 * avg,0.8*avg, 1.0*avg)
    for (int i = 0; i < nloc; i++){
        for (int x = 0; x < img.width; x++){
            int index = (i * padding) + (x * 3);
            double b = loc[index];
            double g = loc[index + 1];
            double r = loc[index + 2];

            b *= (0.9 * avg);
            g *= (0.8 * avg);
            r *= (1.0 * avg);

            loc[index + 0] = (unsigned char)b;
            loc[index + 1] = (unsigned char)g;
            loc[index + 2] = (unsigned char)r;
        }
    }

    MPI_Gatherv(loc, nloc * padding, MPI_UNSIGNED_CHAR, img.bgr.data(), counts, displs, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    if (rank == 0)
        printf("sum=%lld\n", gsum);

    free(loc);
    if (rank == 0)
    {
        save_bmp(output_bmp, &img);
        free(counts);
        free(displs);
    }
    MPI_Finalize();
    return 0;
}