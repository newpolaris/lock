#include <benchmark/benchmark.h>
#include <mutex>
#include <atomic>
#include <cassert>


#if __has_builtin(__builtin_prefetch)
#   define UTILS_PREFETCH( exp ) (__builtin_prefetch(exp))
#else
#   define UTILS_PREFETCH( exp )
#endif

#       define UTILS_WAIT_FOR_INTERRUPT()
#       define UTILS_WAIT_FOR_EVENT()
#       define UTILS_BROADCAST_EVENT()
#       define UTILS_SIGNAL_EVENT()
#       define UTILS_PAUSE()
#       define UTILS_PREFETCHW(addr)        UTILS_PREFETCH(addr)

namespace utils
{
    class SpinLock {
        std::atomic_flag mLock = ATOMIC_FLAG_INIT;

    public:
        void lock() noexcept {
            UTILS_PREFETCHW(&mLock);
#ifdef __ARM_ACLE
            // we signal an event on this CPU, so that the first yield() will be a no-op,
            // and falls through the test_and_set(). This is more efficient than a while { }
            // construct.
            UTILS_SIGNAL_EVENT();
            do {
                yield();
            } while (mLock.test_and_set(std::memory_order_acquire));
#else
            goto start;
            do {
                yield();
            start:;
            } while (mLock.test_and_set(std::memory_order_relaxed));
#endif
        }

        void unlock() noexcept {
            mLock.clear(std::memory_order_relaxed);
#ifdef __ARM_ARCH_7A__
            // on ARMv7a SEL is needed
            UTILS_SIGNAL_EVENT();
            // as well as a memory barrier is needed
            __dsb(0xA);     // ISHST = 0xA (b1010)
#else
            // on ARMv8 we could avoid the call to SE, but we'de need to write the
            // test_and_set() above by hand, so the WFE only happens without a STRX first.
            UTILS_BROADCAST_EVENT();
#endif
        }

    private:
        inline void yield() noexcept {
            // on x86 call pause instruction, on ARM call WFE
            UTILS_WAIT_FOR_EVENT();
        }
    };
}

namespace basic {

    class Spinlock
    {
    public:

        void lock() noexcept {
            while (flag.test_and_set(std::memory_order_acquire))
                ;
        }

        void unlock() noexcept {
            flag.clear(std::memory_order_release);
        }

        std::atomic_flag flag = ATOMIC_FLAG_INIT;
    };
}

namespace jason
{
    class Spinlock
    {
    public:

        void lock()
        {
            const int lock_value = 1;

            while (1)
            {
                if (!atomic.load(std::memory_order_relaxed))
                {
                    int zero = 0;
                    if (atomic.compare_exchange_strong(zero, lock_value, std::memory_order_relaxed))
                        break;
                }
                // _mm_pause();
            }
           // std::atomic_thread_fence(std::memory_order_acquire);
        }

        void unlock()
        {
           // std::atomic_thread_fence(std::memory_order_release);

            atomic.store(0, std::memory_order_relaxed);

           // std::atomic_thread_fence(std::memory_order_seq_cst);
        }

        std::atomic_int atomic{ 0 };
    };
}

static void BM_std_mutex(benchmark::State& state) {
    static std::mutex l;
    for (auto _ : state) {
        l.lock();
        l.unlock();
    }
}


static void BM_spinlock(benchmark::State& state) {
    static jason::Spinlock l;

    for (auto _ : state) {
        l.lock();
        l.unlock();
    }
}

static void BM_basic_spinlock(benchmark::State& state) {
    static basic::Spinlock l;
    for (auto _ : state) {
        l.lock();
        l.unlock();
    }
}

static void BM_util_spinlock(benchmark::State& state) {
    static utils::SpinLock l;
    for (auto _ : state) {
        l.lock();
        l.unlock();
    }
}

static void BM_util2_spinlock(benchmark::State& state) {
    static utils::SpinLock l;
    for (auto _ : state) {
        l.lock();
        l.unlock();
    }
}

BENCHMARK(BM_std_mutex)
->Threads(1)
->Threads(2)
->Threads(8)
->ThreadPerCpu();

BENCHMARK(BM_spinlock)
->Threads(1)
->Threads(2)
->Threads(8)
->ThreadPerCpu();

BENCHMARK(BM_basic_spinlock)
->Threads(1)
->Threads(2)
->Threads(8)
->ThreadPerCpu();

BENCHMARK(BM_util_spinlock)
->Threads(1)
->Threads(2)
->Threads(8)
->ThreadPerCpu();

BENCHMARK(BM_util2_spinlock)
->Threads(1)
->Threads(2)
->Threads(8)
->ThreadPerCpu();

BENCHMARK_MAIN();