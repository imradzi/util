#include <mutex>
#include <chrono>
#include <atomic>
#include "WakeableSleeper.h"

using namespace std::chrono_literals;

template<typename T>
std::tuple<bool, std::unique_lock<T>> LockForever(T& lock, ObservableAtomic& isServerDown, const std::chrono::milliseconds timeOut = 500ms) {
    // Create a sleeper that monitors the flag
    WakeableSleeper sleeper {{{&isServerDown, true}}};

    while (!isServerDown.load()) {
        if (lock.try_lock_for(timeOut)) {
            return {true, std::unique_lock<T>(lock, std::adopt_lock)};
        }
        // Sleep briefly, will wake early if isServerDown changes
        sleeper.sleep_for(std::chrono::milliseconds(10));
    }
    return {false, std::unique_lock<T>()};
}
