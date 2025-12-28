#include <mutex>
#include <chrono>
#include <atomic>

template<typename T>
std::tuple<bool, std::unique_ptr<std::lock_guard<T>>> LockForever(T &lock, std::atomic<bool> &isServerDown, const std::chrono::microseconds timeOut=std::chrono::milliseconds(500)) {
    while (!isServerDown) {
        if (lock.try_lock_for(timeOut)) {
            return {true, std::make_unique<std::lock_guard<T>>(lock, std::adopt_lock)};
        }
    }
    return {false, nullptr};
}


