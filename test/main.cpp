#include <atomic>
#include <vector>
#include <thread>
#include <mutex>

const int loop = 1'000'000;
const int threads = 4;

int k = 0;
bool flag = false;
std::mutex init_mutex;
std::mutex mutex;
std::condition_variable cond;

namespace internal {

// FIXME: wouldn't LTO mess this up?
void UseCharPointer(char const volatile*) {}

}  // namespace internal

#if 1
template <class Tp>
inline __forceinline void DoNotOptimize(Tp const& value) {
    internal::UseCharPointer(&reinterpret_cast<char const volatile&>(value));
    _ReadWriteBarrier();
}
#else
template <class Tp>
inline __forceinline void DoNotOptimize(Tp const& value) {
}
}
#endif

void run()
{
    std::unique_lock<std::mutex> guard(init_mutex);
    cond.wait(guard, [&]() { return flag; });
    guard.unlock();

    for (int i = 0; i < loop; i++) {
        std::lock_guard<std::mutex> lock(mutex);
        DoNotOptimize(k++);
    }
}

int main()
{
    std::vector<std::thread> vec;
    for (int i = 0; i < threads; i++)
        vec.emplace_back(run);
    
    std::unique_lock<std::mutex> guard(init_mutex);
    flag = true;
    guard.unlock();
    cond.notify_all();

    for (auto & th : vec)
        th.join();

    if (k != loop * threads)
        abort();

    return 0;
}