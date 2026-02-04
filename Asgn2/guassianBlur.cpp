// due feb 7 2026
// guassian blur
// ./GaussianBlur [n] [c] [bmp]
// https://www.mpich.org/static/docs/v3.3/www3/

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

/* guassian blur :
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
/* guassian horizontal blur */
void horizontal_blur(uint8_t *data, int rnum, int width, int *next_row, lock_t *m)
{
    int padding = row_padded(width);
    std::vector<uint8_t> rtemp(padding);

    while (true)
    {
        lock(m);
        int sr = *next_row;
        if (sr >= rnum)
        {
            unlock(m);
            break;
        }
        (*next_row)++;
        unlock(m);

        memcpy(rtemp.data(), &data[sr * padding], padding);
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
                    double weight_value = weights[abs(c)];
                    int in = neigh_x * 3;
                    b += rtemp[in + 0] * weight_value;
                    g += rtemp[in + 1] * weight_value;
                    r += rtemp[in + 2] * weight_value;
                    weight_count += weight_value;
                }
            }
            int out = (sr * padding) + (x * 3);
            data[out + 0] = b / weight_count;
            data[out + 1] = g / weight_count;
            data[out + 2] = r / weight_count;
        }
    }
}

/* guassian vertical blur */
// data[col*3 + y*rwb+coloroffset]
void vertical_blur(uint8_t *data, int width, int height, int *next_col, lock_t *m)
{
    int rwb_padding = row_padded(width);

    std::vector<uint8_t> ctemp(height * 3);
    while (true)
    {
        lock(m);
        int col = *next_col;
        if (col >= width)
        {
            unlock(m);
            break;
        }
        (*next_col)++;
        unlock(m);

        for (int y = 0; y < height; y++)
        {
            ctemp[y * 3 + 0] = data[(col * 3) + (y * rwb_padding) + 0];
            ctemp[y * 3 + 1] = data[(col * 3) + (y * rwb_padding) + 1];
            ctemp[y * 3 + 2] = data[(col * 3) + (y * rwb_padding) + 2];
        }

        for (int y = 0; y < height; y++)
        {
            double b = 0.0, g = 0.0, r = 0.0;
            double weight_count = 0.0;

            // offset
            for (int c = -3; c <= 3; c++)
            {
                int neigh_y = y + c;

                // check for corner/bound
                if (neigh_y >= 0 && neigh_y < height)
                {
                    double weight_value = weights[abs(c)];
                    int in = neigh_y * 3;
                    b += ctemp[in + 0] * weight_value;
                    g += ctemp[in + 1] * weight_value;
                    r += ctemp[in + 2] * weight_value;
                    weight_count += weight_value;
                }
            }
            int out = (y * rwb_padding) + (col * 3);
            data[out + 0] = b / weight_count;
            data[out + 1] = g / weight_count;
            data[out + 2] = r / weight_count;
        }
    }
}

// mpi_gatherv - gather from all process in a group
void gather(void *send_buf, int send_count, void *recv_buf, int *recv_count, int *displs)
{
    MPI_Gatherv(send_buf, send_count, MPI_UNSIGNED_CHAR, recv_buf, recv_count, displs, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
}

int main(int argc, char **argv)
{
    // class code: MPI
    MPI_Init(&argc, &argv);
    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);   // rank == id
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs); // total worker

    if (argc < 4)
    {
        printf("Usage: %s <n> <c> <input.bmp> <output.bmp>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    int n = atoi(argv[1]);
    int c = atoi(argv[2]);
    const char *input_bmp = argv[3];
    const char *output_bmp = argv[4];

    BMPImage24 img = load_bmp(input_bmp);

    const int N = img.height;
    int base = N / nprocs;
    int rem = N % nprocs;
    int nloc = base + (rank < rem);

    int *counts = 0, *displs = 0;
    int padding = row_padded(img.width);

    if (rank == 0)
    {
        counts = (int *)calloc(nprocs, sizeof(int));
        displs = (int *)calloc(nprocs, sizeof(int));

        int off = 0;
        for (int p = 0; p < nprocs; p++)
        {
            counts[p] = (base + (p < rem)) * padding;
            displs[p] = off * padding;
            off += base + (p < rem);
        }
    }

    double start = MPI_Wtime();
    uint8_t *loc = (uint8_t *)malloc(nloc * padding);
    lock_t m;
    init(&m);

    for (int i = 0; i < n; i++)
    {
        // if (rank == 0) printf("iteration %d/%d\n", i + 1, n);
        MPI_Scatterv(img.bgr.data(), counts, displs, MPI_UNSIGNED_CHAR, loc, nloc * padding, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
        // if (rank == 1 && i == 0)
        // {
        //     printf("rank 1 received first Byte: %d\n", loc[0]);
        // }

        int next_row = 0;
        std::vector<std::thread> hworkers;
        for (int i = 0; i < c; i++)
        {
            hworkers.emplace_back(horizontal_blur, loc, nloc, img.width, &next_row, &m);
        }
        for (int i = 0; i < c; i++)
        {
            hworkers[i].join();
        }
        // if (rank == 1 && i == 0) printf("horizontal done\n");

        gather(loc, (nloc * padding), img.bgr.data(), counts, displs);

        if (rank == 0)
        {
            int next_col = 0;
            std::vector<std::thread> vworkers;
            for (int i = 0; i < c; i++)
            {
                vworkers.emplace_back(vertical_blur, img.bgr.data(), img.width, img.height, &next_col, &m);
            }
            for (int i = 0; i < c; i++)
            {
                vworkers[i].join();
            }
            // printf("vertical done\n");
        }
    }
    double end = MPI_Wtime();

    if (rank == 0)
    {
        printf("Time: %.4f sec\n", end - start);

        save_bmp(output_bmp, &img);
        printf("output: %s\n", output_bmp);
    }
    free(loc);
    if (rank == 0)
    {
        free(counts);
        free(displs);
    }
    MPI_Finalize();
    return 0;
}