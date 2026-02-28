#include <mutex>
#include <chrono>
#include <atomic>
#include "WakeableSleeper.h"

using namespace std::chrono_literals;

//LockForever: try lock while all the conditions in the checkList are true, 
//   if any of the condition is false, return immediately with false.
template<typename T, typename Rep = long long, typename Period = std::milli>
std::tuple<bool, std::unique_lock<T>> LockForever(T& lock, std::vector<std::pair<ObservableAtomic*, bool>> checkList, const std::chrono::duration<Rep, Period> timeOut = 500ms) {
    // Create a sleeper that monitors the flag
    WakeableSleeper sleeper{checkList};

    auto eval = [&checkList]() {
        return std::all_of(checkList.begin(), checkList.end(), [](const auto& check) {
            return check.first->load() == check.second;
        });
    };

    while (eval()) {
        if (lock.try_lock_for(timeOut)) {
            return {true, std::unique_lock<T>(lock, std::adopt_lock)};
        }
        // Sleep briefly, will wake early if isServerDown changes
        sleeper.sleep_for(std::chrono::milliseconds(10));
    }
    return {false, std::unique_lock<T>()};
}

// helper functor to return lock that will be unlocked when going out of scope, and a bool to indicate if lock is acquired or not.
template<typename T>
std::tuple<bool, std::unique_lock<T>> TryLock(T& lock) {
    if (lock.try_lock()) return {true, std::unique_lock<T>(lock, std::adopt_lock)};
    return {false, std::unique_lock<T>()};
}
