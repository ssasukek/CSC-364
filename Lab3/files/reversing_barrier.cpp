// sense reversing barrier
#include <stdatomic.h>
#include <atomic>
#include <thread>

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

    if (position == barrier->nthreads - 1){
        barrier->count.store(0,  std::memory_order_release); 
        barrier->gate.store(!my_gate, std::memory_order_release); // open the gate
    } else {
        // wait until gate is opened
        while (barrier->gate.load(std::memory_order_acquire) == my_gate){
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

static inline void rbarrier_wait(struct SenseReversingBarrier *barrier, int *local_sense){
    *local_sense = !(*local_sense); // flip local sense
    int ls = *local_sense;

    int prev = atomic_fetch_sub(&(barrier->count), 1);
    if (prev == 1){
        barrier->count.store(barrier->nthreads, std::memory_order_release); 
        barrier->sense.store(ls, std::memory_order_release);
    } else {
        // wait until global sense equals local sense
        while (barrier->sense.load(std::memory_order_acquire) != ls){
            sched_yield(); // yield to other threads
        }
    }
}