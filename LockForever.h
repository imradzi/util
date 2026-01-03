#include <mutex>
#include <chrono>
#include <atomic>

using namespace std::chrono_literals;

template<typename T>
std::tuple<bool, std::unique_lock<T>> LockForever(T &lock, std::atomic<bool> &isServerDown, const std::chrono::milliseconds timeOut = 500ms) {
    while (!isServerDown.load(std::memory_order_acquire)) {
        if (lock.try_lock_for(timeOut)) {
            return {true, std::unique_lock<T>(lock, std::adopt_lock)};
        }
    }
    return {false, std::unique_lock<T>()};
}


