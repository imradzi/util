#pragma once
#include <boost/asio.hpp>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

class FlagNotifier {
    std::mutex mtx;
    std::vector<std::condition_variable*> subscribers;

public:
    void subscribe(std::condition_variable* cv);
    void unsubscribe(std::condition_variable* cv);
    void notify_change();
};

class ObservableAtomic {
    std::atomic<bool> value;
    FlagNotifier notifier;

public:
    ObservableAtomic(bool initial = false) : value(initial) {}

    void store(bool v);
    bool load() const;
    FlagNotifier* get_notifier() { return &notifier; }
    std::atomic<bool>* get_atomic() { return &value; }
};

// Improved WakeableSleeper
// sleep for duration or until any of the flags in the list is set to the specified value, whichever comes first. 
//      Returns true if woke up by flag change, false if timeout.
class WakeableSleeper {
    std::mutex cv_mtx;
    std::condition_variable cv;
    std::vector<std::pair<std::atomic<bool>*, bool>> flags;
    std::vector<FlagNotifier*> notifiers;

public:
    WakeableSleeper(std::vector<std::pair<ObservableAtomic*, bool>> f);
    ~WakeableSleeper();

    // sleep_for -> true if timeout, false if woken up by flag change
    template<typename Rep, typename Period>
    bool sleep_for(const std::chrono::duration<Rep, Period>& dur) {
        std::unique_lock<std::mutex> lock(cv_mtx);
        bool result = false;
        cv.wait_for(lock, dur, [&] {
            result = std::any_of(flags.begin(), flags.end(), [](const auto& ptr) {
                return ptr.first && (ptr.first->load() == ptr.second);
            });
            return result;
        });
        return !result;
    }
};


// Usage becomes cleaner
// ObservableAtomic shutdown {false};
// ObservableAtomic paused {false};

// WakeableSleeper sleeper {
//     {{&shutdown, true}, {&paused, true}}};

// // Notification is automatic
// shutdown.store(true);  // Automatically notifies all sleepers