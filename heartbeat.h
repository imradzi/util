#pragma once
#include <boost/asio.hpp>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

class HeartBeat {
    std::mutex mtx;
    std::condition_variable cv;
    std::function<void(boost::system::error_code)> timer_handler;
    std::atomic<uint64_t> beatCounter {0};
    std::atomic<bool> isPaused {false};
    std::atomic<bool> isShutdown {false};
    boost::asio::io_context io;
    boost::asio::steady_timer timer;
    std::thread thread;
public:
    HeartBeat(std::chrono::milliseconds milliSeconds);
    ~HeartBeat();
    void Wait();
    void Stop();
    void Beat(bool stopPause = true);
    void Pause();
};