// Two Stage Tone Mapping Operator
// 1. Compoute one global brightness value for the entire image
// 2. Use that value to tone-map every pixel
// 4 threads + two stage mapping + gather function
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <stdatomic.h>
#include <atomic>
#include <thread>
#include <math.h>

// 4 threads
#define THREADS 4


// gather()
static std::atomic_int gv[THREADS]; // global value for each thread

static void gather(int gather_id, int id, int tc)
{
    gv[id].fetch_add(1, std::memory_order_acq_rel);

    while (1)
    {
        int break_flag = 1;
        for (int i = 0; i < tc; i++)
        {
            if (gv[i].load(std::memory_order_acquire) < gather_id)
                break_flag = 0;
        }
        if (break_flag)
            break;
        std::this_thread::yield(); // yield to other threads
    }
}

struct ThreadData
{
    int id;
    int tc;
    size_t start_pixel;
    size_t end_pixel;
    int local_sense;
    BMPImage24 *img;
};

static void threadfct(ThreadData *td)
{
    BMPImage24 &img = *(td->img);

    // Stage 1: compute global avg brightness sum
    const double a = 0.18; // exposure key value

    // Compute Luminance: L = 0.2126 R + 0.7152 G + 0.0722 B
    double global_sum = 0.0;
    for (size_t i = td->start_pixel; i < td->end_pixel; i++)
    {
        uint8_t r = img.rgb[i * 3 + 0];
        uint8_t g = img.rgb[i * 3 + 1];
        uint8_t b = img.rgb[i * 3 + 2];
        double L = 0.2126 * r + 0.7152 * g + 0.0722 * b;
        global_sum += log(L + 1);
    }

    gather(1, td->id, td->tc);

    // Lavg = exp(S / N) - 1
    if (td->id == 0)
    {
        double S = 0.0;
        for (int i = 0; i < td->tc; i++)
        {
            S += gv[i].load(std::memory_order_acquire);
        }
        double N = img.width * img.height;
        double Lavg = exp(S / N) - 1;
        gv[0].store(Lavg, std::memory_order_release);
    }

    gather(2, td->id, td->tc);

    // Stage 2: tone map each pixel (Reinhard Operator))
    // L = 0.2126 R + 0.7152 G + 0.0722 B
    // Lm = (a / Lavg) * L
    // Ld = Lm / (1 + Lm)
    for (size_t i = td->start_pixel; i < td->end_pixel; i++)
    {
        uint8_t r = img.rgb[i * 3 + 0];
        uint8_t g = img.rgb[i * 3 + 1];
        uint8_t b = img.rgb[i * 3 + 2];
        double L = 0.2126 * r + 0.7152 * g + 0.0722 * b;

        double Lavg = gv[0].load(std::memory_order_acquire);
        double Lm = (a / Lavg) * L;
        double Ld = Lm / (1.0 + Lm);

        double scale = (L > 0) ? (Ld / L) : 0.0;

        uint8_t r_new = (uint8_t)fmin(fmax(r * scale, 0.0), 255.0);
        uint8_t g_new = (uint8_t)fmin(fmax(g * scale, 0.0), 255.0);
        uint8_t b_new = (uint8_t)fmin(fmax(b * scale, 0.0), 255.0);

        img.rgb[i * 3 + 0] = r_new;
        img.rgb[i * 3 + 1] = g_new;
        img.rgb[i * 3 + 2] = b_new;
    }

    gather(3, td->id, td->tc);
}

static void tone_mapping(BMPImage24 *img, int use_sense)
{
    std::thread threads[THREADS];
    ThreadData td[THREADS];

    size_t total_pixels = img->width * img->height;
    size_t pixels_per_thread = (total_pixels + THREADS - 1) / THREADS;

    // Initialize gv
    for (int i = 0; i < THREADS; i++)
    {
        gv[i].store(0, std::memory_order_release);
    }

    // Create threads
    for (int i = 0; i < THREADS; i++)
    {
        td[i].id = i;
        td[i].tc = THREADS;
        td[i].start_pixel = i * pixels_per_thread;
        td[i].end_pixel = std::min((i + 1) * pixels_per_thread, total_pixels);

        td[i].img = img;
        threads[i] = std::thread(threadfct, &td[i]);
    }

    // Join threads
    for (int i = 0; i < THREADS; i++)
    {
        threads[i].join();
    }
}

// #include "reversing_barrier.cpp"
// #include "bmp.cpp"