#include "WakeableSleeper.h"
#include "logging.hpp"

void FlagNotifier::subscribe(std::condition_variable* cv) const {
    std::lock_guard<std::mutex> lock(mtx);
    subscribers.push_back(cv);
}

void FlagNotifier::unsubscribe(std::condition_variable* cv) const {
    std::lock_guard<std::mutex> lock(mtx);
    subscribers.erase(std::remove(subscribers.begin(), subscribers.end(), cv),
        subscribers.end());
}

void FlagNotifier::notify_change() const {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto* cv : subscribers) {
        cv->notify_all();
    }
}

void ObservableAtomic::store(bool v) {
    bool old = value.exchange(v, std::memory_order_acq_rel);
    if (old != v) {  // Only notify on actual change
        notifier.notify_change();
    }
}

bool ObservableAtomic::load() const {
    return value.load(std::memory_order_acquire);
}

WakeableSleeper::WakeableSleeper(std::vector<std::pair<const ObservableAtomic*, bool>> f) {
    for (auto& x : f) {
        flags.emplace_back(x.first->get_atomic(), x.second);
        auto notify = notifiers.emplace_back(x.first->get_notifier());
        notify->subscribe(&cv);
    }
}

WakeableSleeper::~WakeableSleeper() {
    // Unsubscribe from all notifiers
    for (auto* notifier : notifiers) {
        notifier->unsubscribe(&cv);
    }
}
