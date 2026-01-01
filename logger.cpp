#include "precompiled/libcommon.h"
#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#ifdef _WIN32
#include "winsock2.h"
#endif

#ifdef __clang__
#if __has_warning("-Wdeprecated-enum-enum-conversion")
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"  // warning: bitwise operation between different enumeration types ('XXXFlags_' and 'XXXFlagsPrivate_') is deprecated
#endif
#endif

#define DONT_USE_ALLOCATION_TRACKER 1

#if defined(__WXWINDOWS_) || defined(__WXGTK__) || defined(__WXMSW__)
#include "wx/wxprec.h"
#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif
#include "wx/stdstream.h"
#include "wx/wfstream.h"
#include "wx/log.h"
#endif

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <format>
#include <mutex>

#include "guid.h"
#include "logger.h"

std::size_t Logger::GetThreadIndex(const std::thread::id id) {
    static std::mutex my_mutex;
    static std::size_t nextindex = 0;
    static std::unordered_map<std::thread::id, std::size_t> ids;
    std::lock_guard<std::mutex> lock(my_mutex);
    auto iter = ids.find(id);
    if (iter == ids.end())
        return ids[id] = nextindex++;
    return iter->second;
}


const char *Logger::timeFormat = "%H:%M:%S";
const char *Logger::dateFormat = "%Y-%m-%d %a (%W)";

bool Logger::toShowMillisecond = true;
bool Logger::showAbsTime = true;
bool Logger::showThreadID = true;
std::string loggerAppCode;

std::string Logger::GetThreadID() {
    auto idx = GetThreadIndex(std::this_thread::get_id());
    return fmt::format("{:03d}", idx);
}

static bool logThreadWasAlive = false;

bool ShowLog(const std::string &t) {
    if (Logger::IsLogActive()) {
        Logger::logMessage(t);
        logThreadWasAlive = true;
        return true;
    } else {
        std::cout << t << std::endl;
    }
    if (logThreadWasAlive) {
        return true;
    }
    return false;
}

LoggerTracker::LoggerTracker(const std::string &m, bool showOnlyEnding) : msg(m) {
    tStart = std::chrono::steady_clock::now();
    if (!showOnlyEnding) {
        Logger::logMessage(fmt::format("{} started.", msg));
    }
}
LoggerTracker::~LoggerTracker() {
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart);
    Logger::logMessage(fmt::format("{} completed. Elapsed {} ms.", msg, ts.count()));
}

LoggerTrackerStreamOut::LoggerTrackerStreamOut(const std::string &m, bool showOnlyEnding) : msg(m) {
    tStart = std::chrono::steady_clock::now();
    if (!showOnlyEnding) {
        std::cout << msg << " started.\n";
    }
}

LoggerTrackerStreamOut::~LoggerTrackerStreamOut() {
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart);
    std::cout << msg << " completed. Elapsed " << ts.count() << " ms.\n";
}

TimeTrackerStream::TimeTrackerStream(const std::string &m, std::ostream &x, bool repInms) : msg(m),
                                                                                            out(x),
                                                                                            reportInMilliseconds(repInms) {
    tStart = std::chrono::steady_clock::now();
    out << msg << " started.\n";
}
void TimeTrackerStream::Write(const std::string &s) {
    auto ts = std::chrono::steady_clock::now() - tStart;
    out << fmt::format("{} Elapsed in {}.", s, reportInMilliseconds ? (std::chrono::duration_cast<std::chrono::milliseconds>(ts)).count() : (std::chrono::duration_cast<std::chrono::seconds>(ts)).count()) << '\n';
}
TimeTrackerStream::~TimeTrackerStream() {
    auto ts = std::chrono::steady_clock::now() - tStart;
    out << fmt::format("{} Completed in {}.", msg, reportInMilliseconds ? (std::chrono::duration_cast<std::chrono::milliseconds>(ts)).count() : (std::chrono::duration_cast<std::chrono::seconds>(ts)).count()) << '\n';
}

AllocationTracker gAllocationTracker;

#ifndef DONT_USE_ALLOCATION_TRACKER
void *operator new(size_t sz) {
    gAllocationTracker.AddMemory(sz);
    return (void *)malloc(sz);
}

void *operator new[](size_t sz) {
    gAllocationTracker.AddMemory(sz);
    return (void *)malloc(sz);
}

void operator delete(void *m, size_t sz) {
    gAllocationTracker.ReleaseMemory(sz);
    free(m);
}

void operator delete[](void *m, size_t sz) {
    gAllocationTracker.ReleaseMemory(sz);
    free(m);
}

void operator delete[](void *m) {
    free(m);
}
void operator delete(void *m) {
    free(m);
}
#endif

void AllocationTracker::Summarize(bool all) {
    std::cout << "\nMemory usage: \n";
    if (all) {
        std::cout << "Total memory allocated  " << std::setiosflags(std::ios::right) << std::setw(12) << allocated << std::endl;
        std::cout << "Total memory deallocated" << std::setiosflags(std::ios::right) << std::setw(12) << deallocated << std::endl;
    }
    std::cout << "Total memory unreleased " << std::setiosflags(std::ios::right) << std::setw(12) << GetMemoryUsage() << std::endl;
    std::cout << "Max usage               " << std::setiosflags(std::ios::right) << std::setw(12) << max << std::endl;
}
