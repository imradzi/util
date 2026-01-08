#include "heartbeat.h"

using namespace std::chrono_literals;

HeartBeat::HeartBeat(std::chrono::milliseconds milliSec) : io(), timer(io) {
    // Run Asio in separate thread
    thread = std::thread([&](std::chrono::milliseconds dur) {
        timer_handler = [&](boost::system::error_code ec) {
            if (ec) return;
            Beat(false); // don't stop pause
            timer.expires_after(dur);
            timer.async_wait(timer_handler);
        };

        timer.expires_after(dur);
        timer.async_wait(timer_handler);

        io.run();
    }, milliSec);
}

HeartBeat::~HeartBeat() {
    Stop();
}


void HeartBeat::Beat(bool stopPause) {
    {
        std::lock_guard lock(mtx);
        beatCounter.fetch_add(1, std::memory_order_release);
        if (stopPause) isPaused = false;
    }
    cv.notify_all();
}

void HeartBeat::Wait() {
    std::unique_lock lock(mtx);
    uint64_t currentBeat = beatCounter.load(std::memory_order_acquire);
    cv.wait(lock, [&] {
        return isShutdown.load(std::memory_order_acquire) || 
               (!isPaused.load(std::memory_order_acquire) && beatCounter.load(std::memory_order_acquire) > currentBeat);
    });
}

void HeartBeat::Pause() {
    {
        std::lock_guard lock(mtx);
        isPaused = true;
    }
    cv.notify_all();
}

void HeartBeat::Stop() {
    timer.cancel();
    isShutdown.store(true, std::memory_order_release);
    Beat(); // last beat to wake all waiting threads
    io.stop();
    thread.join();
}

