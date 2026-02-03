// due feb 7 2026
// guassian blur
// ./GaussianBlur [n] [c] [bmp]

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

/* class code mutex */
/* mutex is a synchronization primitive that enforeces limit
    on access to a shared resource when having multiple thread of execution
*/
typedef struct
{
    std::atomic_flag f;
} lock_t;
static void init(lock_t *m)
{
    atomic_flag_clear(&m->f); // f=0
}
static void lock(lock_t *m) // t1t2
{
    while (atomic_flag_test_and_set(&m->f))
    {
        // spin
    }
}
static void unlock(lock_t *m)
{
    atomic_flag_clear(&m->f);
}

void gather()
{
}

void vertical_blur(const BMPImage24 *src, BMPImage24 *dst, int *shared_row, lock_t *m){

}

void horizontal_blur(const BMPImage24 *src, BMPImage24 *dst, int *shared_row, lock_t *m)
{
    int width = src->width;
    int height = src->height;
    int padding = row_padded(width);

    while (true)
    {
        lock(m);
        int sr = *shared_row;
        if (sr >= src->height)
        {
            unlock(m);
            break;
        }
        (*shared_row)++;
        unlock(m);

        /* guassian horizontal blur :
        p[i] = p[i] * 0.399050
        + p[i +1] *(0.242036) + p[i +2] *(0.054005) + p[i +3] *(0.004433)
        + p[i -1] *(0.242036) + p[i -2] *(0.054005) + p[i -3] *(0.004433);
        */
        const double weights[] = {
            0.399050,
            0.242036,
            0.054005,
            0.004433
        };

        for (int x = 0; x < width; x++)
        {
            double weight_count = 0.0;
            double b = 0.0, g = 0.0, r = 0.0;

            // offset
            for (int c = -3; c <= 3; c++)
            {
                int neigh_x = x + c;

                // check for corner/bound
                if (neigh_x >= 0 && neigh_x < width)
                {
                    int pixel_idx = (sr * padding) + (neigh_x * 3);
                    double weight_value = weights[abs(c)];
                    b += src->bgr[pixel_idx + 0] * weight_value;
                    g += src->bgr[pixel_idx + 1] * weight_value;
                    r += src->bgr[pixel_idx + 2] * weight_value;
                    weight_count += weight_value;
                }
            }
            int out_res = (sr * padding) + (x * 3);
            dst->bgr[out_res + 0] = b / weight_count;
            dst->bgr[out_res + 1] = g / weight_count;
            dst->bgr[out_res + 2] = r / weight_count;
        }
        /* guassian vertical blur */
    }
}

int main(int argc, char **argv)
{
    // class code: MPI
    MPI_Init(&argc, &argv);

    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // rank == id
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs); // total worker

    if (argc < 3)
    {
        printf("Usage: %s <input.bmp> <output.bmp>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    const char *input_bmp = argv[1];
    const char *output_bmp = argv[2];

    BMPImage24 img = load_bmp(input_bmp);
    BMPImage24 out_img = img;

    const int N = 10000;
    int *arr = 0;

    int base = N / nprocs, rem = N % nprocs;
    int nloc = base + (rank < rem);

    int *counts = 0, *displs = 0;
    if (rank == 0)
    {
        arr = (int *)malloc(N * sizeof(int));
        srand(1);
        for (int i = 0; i < N; i++)
            arr[i] = rand() % 1000;

        counts = (int *)malloc(nprocs * sizeof(int));
        displs = (int *)malloc(nprocs * sizeof(int));

        int off = 0;
        for (int p = 0; p < nprocs; p++)
        {
            counts[p] = base + (p < rem);
            displs[p] = off;
            off += counts[p];
        }
    }

    int *loc = (int *)malloc(nloc * sizeof(int));

    MPI_Scatterv(arr, counts, displs, MPI_INT, loc, nloc, MPI_INT, 0, MPI_COMM_WORLD);

    for (int i = 0; i < nloc; i++)
        loc[i]++;

    // per-entry arithmetic op

    MPI_Barrier(MPI_COMM_WORLD); // barrier between phases

    long long lsum = 0;
    for (int i = 0; i < nloc; i++)
        lsum += loc[i];

    long long gsum = 0;
    MPI_Reduce(&lsum, &gsum, 1, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0)
        printf("sum=%lld\n", gsum);
        save_bmp(output_bmp, &out_img);
        printf("output: %s\n", output_bmp);

    free(loc);
    if (rank == 0)
    {
        free(arr);
        free(counts);
        free(displs);
    }
    MPI_Finalize();
    return 0;
}