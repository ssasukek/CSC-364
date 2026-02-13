// mpiexec -n 4 ./halo image.bmp
// new[y][x]=0.25⋅old[y][x]+0.1875⋅(old[y−1][x]+old[y+1][x]+old[y][x−1]+old[y][x+1])

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

    if (argc < 3)
    {
        printf("Usage: mpiexec -n 4 <n> ./halo.exe <input.bmp> <output.bmp>\n", argv[0]);
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

    // allocate mem for loc chunk and ghost row (top n bot)
    int row_alloc = nloc + 2;
    unsigned char *loc_old = (unsigned char *)malloc(row_alloc * padding);
    unsigned char *loc_new = (unsigned char *)malloc(nloc * padding);
    memset(loc_old, 0, row_alloc * padding);

    MPI_Scatterv(img.bgr.data(), counts, displs, MPI_UNSIGNED_CHAR, loc_old + padding, nloc * padding, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    if (rank == 0) printf("rank 0 sending %d to rank 1\n", loc_old[nloc * padding + (img.width / 2) * 3]);
    // Halo Exchange
    int top_neigh = rank - 1;
    int bot_neigh = rank + 1;
    if (rank == 0){
        top_neigh = MPI_PROC_NULL;
    }
    if (rank == nprocs - 1){
        bot_neigh = MPI_PROC_NULL;
    }

    // exchange to bottom ghost row
    MPI_Sendrecv(loc_old + padding, padding, MPI_UNSIGNED_CHAR, top_neigh, 0, loc_old + (nloc + 1) * padding, padding, MPI_UNSIGNED_CHAR, bot_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    printf("exchange 1 pass\n");
    // exchange to top ghost row
    MPI_Sendrecv(loc_old + (nloc * padding), padding, MPI_UNSIGNED_CHAR, bot_neigh, 1, loc_old, padding, MPI_UNSIGNED_CHAR, top_neigh, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    printf("exchange 2 pass\n");

    if (rank == 1)
        printf("rank 1 received %d from rank 0\n", loc_old[nloc * padding + (img.width / 2) * 3]);

    // Stencil iteration 1
    for (int y = 1; y <= nloc; y++){
        for (int x = 0; x < img.width * 3; x++){
            unsigned char* curr_row = loc_old + (y * padding);
            unsigned char* up = loc_old + ((y + 1) * padding);
            unsigned char* down = loc_old + ((y - 1) * padding);

            // stencil formula : new[y][x]=0.25⋅old[y][x]+0.1875⋅(old[y−1][x]+old[y+1][x]+old[y][x−1]+old[y][x+1])
            double center = curr_row[x];
            double neighbors = up[x] + down[x] + curr_row[x - 3] + curr_row[x + 3];
            double res = 0.25 * center + 0.1875 * neighbors;
            
            loc_new[(y - 1) * padding + x] = res;
        }
    }

    // checksum
    long long gsum = 0;
    for (int i = 0; i < nloc; i++){
        for (int j = 0; j <img.width * 3; j++){
            gsum += loc_new[i * padding + j];
        }
    }

    long long lsum = 0;
    MPI_Reduce(&gsum, &lsum, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Gatherv(loc_new, nloc * padding, MPI_UNSIGNED_CHAR, img.bgr.data(), counts, displs, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    if (rank == 0)
        printf("gsum=%lld\n", gsum);
    free(loc_old);
    free(loc_new);
    if (rank == 0)
    {
        save_bmp(output_bmp, &img);
        free(counts);
        free(displs);
    }
    MPI_Finalize();
    return 0;
}