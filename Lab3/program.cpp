// due jan 23, 2026
// 4 threqads + two stage tone mapping + barrier + gather fucntion
// /program <input.bmp> <output.bmp>

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
        printf("Error: failed reading BMP headers\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("BMP Headers read successfully\n");
    }

    if (fh.bfType != 0x4D42 || ih.biBitCount != 24 || ih.biCompression != 0)
    {
        printf("Error: only uncompressed 24-bit BMP supported\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("BMP Image: %dx%d\n", ih.biWidth, ih.biHeight);
    }

    BMPImage24 img;
    img.width = ih.biWidth;
    img.height = (ih.biHeight > 0) ? ih.biHeight : -ih.biHeight; // handle top-down BMP
    img.pre_height = ih.biHeight;

    int strde = row_padded(img.width);
    img.rgb.assign((size_t)strde * img.height, 0);

    if (fseek(file, fh.bfOffBits, SEEK_SET) != 0)
    {
        printf("Error seeking to pixel data\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Reading pixel data...\n");
    }

    size_t image_bytes = (ih.biSizeImage != 0) ? (size_t)ih.biSizeImage : (size_t)strde * (size_t)img.height;
    std::vector<uint8_t> data(image_bytes);
    if (fread(data.data(), 1, image_bytes, file) != image_bytes)
    {
        printf("Error reading pixel data\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Pixel data read successfully\n");
    }
    fclose(file);

    for (int row = 0; row < img.height; row++)
    {
        const uint8_t *src = &data[(size_t)row * (size_t)strde];
        uint8_t *dst = &img.rgb[(size_t)row * (size_t)strde];

        // convert only real pixels
        for (int col = 0; col < img.width; col++)
        {
            uint8_t b = src[col * 3 + 0];
            uint8_t g = src[col * 3 + 1];
            uint8_t r = src[col * 3 + 2];
            dst[col * 3 + 0] = r;
            dst[col * 3 + 1] = g;
            dst[col * 3 + 2] = b;
        }
    }
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
    int strde = row_padded(width);

    struct BMPFileHeader fh;
    memset(&fh, 0, sizeof(fh));
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
    fh.bfSize = fh.bfOffBits + (unsigned int)(strde * height);

    struct BMPInfoHeader ih;
    memset(&ih, 0, sizeof(ih));
    ih.biSize = sizeof(BMPInfoHeader);
    ih.biWidth = width;
    ih.biHeight = height;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = 0;
    ih.biSizeImage = (unsigned int)(strde * height);

    fwrite(&fh, sizeof(fh), 1, file);
    fwrite(&ih, sizeof(ih), 1, file);

    std::vector<uint8_t> outrow((size_t)strde, 0);

    for (int row = 0; row < height; row++)
    {
        const uint8_t *src = &img->rgb[(size_t)row * (size_t)strde];
        memset(outrow.data(), 0, (size_t)strde);

        // Convert RGB->BGR for BMP
        for (int col = 0; col < width; col++)
        {
            uint8_t r = src[col * 3 + 0];
            uint8_t g = src[col * 3 + 1];
            uint8_t b = src[col * 3 + 2];
            outrow[col * 3 + 0] = b;
            outrow[col * 3 + 1] = g;
            outrow[col * 3 + 2] = r;
        }

        fwrite(outrow.data(), 1, (size_t)strde, file);
    }

    fclose(file);
}

static void free_image(struct BMPImage24 *img)
{
    img->rgb.clear();
    img->width = 0;
    img->height = 0;
    img->pre_height = 0;
}

// reversing_barrier.cpp

// DIY Gate Barrier
struct DIYGateBarrier
{
    int nthreads; // number of threads
    std::atomic<int> count{0};
    std::atomic<bool> gate{0};
};

static inline void diy_gate_barrier_init(struct DIYGateBarrier *barrier, int nthreads)
{
    barrier->nthreads = nthreads;
    barrier->count.store(0, std::memory_order_relaxed);
    barrier->gate.store(0, std::memory_order_relaxed);
}

static inline void diy_gate_barrier_wait(struct DIYGateBarrier *barrier)
{
    int my_gate = barrier->gate.load(std::memory_order_relaxed);
    int position = atomic_fetch_add(&(barrier->count), 1);

    if (position == barrier->nthreads - 1)
    {
        barrier->count.store(0, std::memory_order_release);
        barrier->gate.store(!my_gate, std::memory_order_release); // open the gate
    }
    else
    {
        // wait until gate is opened
        while (barrier->gate.load(std::memory_order_acquire) == my_gate)
        {
            std::this_thread::yield(); // yield to other threads
        }
    }
}

// Sense Reversing Barrier
struct SenseReversingBarrier
{
    int nthreads; // number of threads
    std::atomic<int> count{0};
    std::atomic<int> sense{0}; // global sense
};

static inline void rbarrier_init(struct SenseReversingBarrier *barrier, int nthreads)
{
    barrier->nthreads = nthreads;
    barrier->count.store(nthreads, std::memory_order_relaxed);
    barrier->sense.store(0, std::memory_order_relaxed); // initial global sense is 0
}

static inline void rbarrier_wait(struct SenseReversingBarrier *barrier, int *local_sense)
{
    *local_sense = !(*local_sense); // flip local sense
    int ls = *local_sense;

    int prev = atomic_fetch_sub(&(barrier->count), 1);
    if (prev == 1)
    {
        barrier->count.store(barrier->nthreads, std::memory_order_release);
        barrier->sense.store(ls, std::memory_order_release);
    }
    else
    {
        // wait until global sense equals local sense
        while (barrier->sense.load(std::memory_order_acquire) != ls)
        {
            std::this_thread::yield(); // yield to other threads
        }
    }
}

// tone_mapping.cpp
static DIYGateBarrier g_diy;
static SenseReversingBarrier g_sense;

static double g_partial_sums[4]; // THREADS = 4
static double g_Lavg = 1.0;

// gather function
static inline void gather(int use_sense, int *local_sense)
{
    if (use_sense == 0)
    {
        rbarrier_wait(&g_sense, local_sense);
    }
    else
    {
        diy_gate_barrier_wait(&g_diy);
    }
}

struct ThreadData
{
    int id;
    int tc;
    size_t start_pixel;
    size_t end_pixel;
    int local_sense;
    int use_sense; // 0: sense reversing barrier, 1: diy gate barrier
    BMPImage24 *img;
};

static void threadfct(ThreadData *td)
{
    BMPImage24 &img = *(td->img);

    // Stage 1: compute global avg brightness sum
    const double a = 0.18; // exposure key value

    int strde = row_padded(img.width);
    // Compute Luminance: L = 0.2126 R + 0.7152 G + 0.0722 B
    double global_sum = 0.0;
    for (size_t i = td->start_pixel; i < td->end_pixel; i++)
    {
        size_t row = i / (size_t)img.width;
        size_t col = i % (size_t)img.width;
        size_t off = row * (size_t)strde + col * 3;

        double r = img.rgb[off + 0] / 255.0;
        double g = img.rgb[off + 1] / 255.0;
        double b = img.rgb[off + 2] / 255.0;

        double L = 0.2126 * r + 0.7152 * g + 0.0722 * b;
        global_sum += log(L + 1.0);
    }
    g_partial_sums[td->id] = global_sum;

    gather(td->use_sense, &td->local_sense);

    // Lavg = exp(S / N) - 1
    if (td->id == 0)
    {
        double S = 0.0;
        for (int i = 0; i < td->tc; i++)
        {
            S += g_partial_sums[i];
        }
        double N = img.width * img.height;
        double Lavg = exp(S / N) - 1.0;

        g_Lavg = Lavg;
        printf("Computed Lavg: %f\n", g_Lavg);
    }

    gather(td->use_sense, &td->local_sense);

    // Stage 2: tone map each pixel (Reinhard Operator))
    // L = 0.2126 R + 0.7152 G + 0.0722 B
    // Lm = (a / Lavg) * L
    // Ld = Lm / (1 + Lm)
    for (size_t i = td->start_pixel; i < td->end_pixel; i++)
    {
        size_t row = i / (size_t)img.width;
        size_t col = i % (size_t)img.width;
        size_t off = row * (size_t)strde + col * 3;

        double r = img.rgb[off + 0] / 255.0;
        double g = img.rgb[off + 1] / 255.0;
        double b = img.rgb[off + 2] / 255.0;
        double L = 0.2126 * r + 0.7152 * g + 0.0722 * b;

        double Lavg = g_Lavg;
        double Lm = (a / Lavg) * L;
        double Ld = Lm / (1.0 + Lm);

        double scale = (L > 0) ? (Ld / L) : 0.0;

        double r2 = r * scale * 255.0;
        double g2 = g * scale * 255.0;
        double b2 = b * scale * 255.0;

        uint8_t r_new = (uint8_t)fmin(fmax(r2, 0.0), 255.0);
        uint8_t g_new = (uint8_t)fmin(fmax(g2, 0.0), 255.0);
        uint8_t b_new = (uint8_t)fmin(fmax(b2, 0.0), 255.0);

        img.rgb[off + 0] = r_new;
        img.rgb[off + 1] = g_new;
        img.rgb[off + 2] = b_new;

    }

    gather(td->use_sense, &td->local_sense);
}

static void tone_mapping(BMPImage24 *img, int use_sense)
{
    std::thread threads[4];
    ThreadData td[4];

    size_t total_pixels = img->width * img->height;
    size_t pixels_per_thread = (total_pixels + 4 - 1) / 4;

    if (use_sense == 0)
    {
        rbarrier_init(&g_sense, 4);
    }
    else
    {
        diy_gate_barrier_init(&g_diy, 4);
    }

    printf("Total pixels: %zu, Pixels per thread: %zu\n", total_pixels, pixels_per_thread);
    // Initialize g_partial_sums
    for (int i = 0; i < 4; i++)
    {
        g_partial_sums[i] = 0.0;
    }
    g_Lavg = 1.0;

    printf("Initializing barrier...\n");
    // Create threads
    for (int i = 0; i < 4; i++)
    {
        td[i].id = i;
        td[i].tc = 4;
        td[i].start_pixel = (size_t)i * pixels_per_thread;
        td[i].end_pixel = std::min((size_t)(i + 1) * pixels_per_thread, total_pixels);
        td[i].local_sense = 0;
        td[i].use_sense = use_sense;
        td[i].img = img;

        threads[i] = std::thread(threadfct, &td[i]);
    }

    printf("Barrier initialized. Waiting for threads to complete...\n");
    // Join threads
    for (int i = 0; i < 4; i++)
    {
        threads[i].join();
    }

    printf("Tone mapping completed.\n");
}

static int parse_mode(int argc, char **argv)
{
    if (argc >= 4)
    {
        if (strcmp(argv[3], "sense") == 0)
        {
            return 0;
        }
        else if (strcmp(argv[3], "diy") == 0)
        {
            return 1;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: %s <input.bmp> <output.bmp> [mode]\n", argv[0]);
    }

    int use_sense = parse_mode(argc, argv);

    printf("Using %s barrier\n", (use_sense == 0) ? "sense reversing" : "DIY gate");

    BMPImage24 img = load_bmp(argv[1]);
    tone_mapping(&img, use_sense);
    save_bmp(argv[2], &img);
    free_image(&img);
    return 0;
}