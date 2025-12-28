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
        isTriggered = true;
        if (stopPause) isPaused = false;
    }
    cv.notify_all();
}

void HeartBeat::Wait() {
    std::unique_lock lock(mtx);
    isTriggered = false;
    cv.wait(lock, [&] {
        return !isPaused.load() && isTriggered.load();
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
    Beat(); // last beat... 
    io.stop();
    thread.join();
}

