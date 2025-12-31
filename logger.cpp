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

#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/pattern_formatter.h>

#include "guid.h"
#include "logger.h"

extern std::size_t GetThreadIndex(const std::thread::id id);

const char *Logger::timeFormat = "%H:%M:%S";
const char *Logger::dateFormat = "%Y-%m-%d %a (%W)";

#if defined(__WXWINDOWS_) || defined(__WXGTK__) || defined(__WXMSW__)
#ifndef NDEBUG
void wpAssertHandler(const wxString &file, int line, const wxString &func, const wxString &cond, const wxString &msg) {
    std::stringstream ss;
    ss << "wxAssert: " << file << ":" << line << " " << func << "(" << cond << ") => " << msg;
    ShowLog(ss.str());
}
#else
void wpAssertHandler(const wxString &, int, const wxString &, const wxString &, const wxString &) {}
#endif
#endif

// empty filename -> output to cout;
bool Logger::toShowMillisecond = true;
bool Logger::showAbsTime = true;
bool Logger::showThreadID = true;
std::string loggerAppCode;

Logger::Logger(const std::string &fname, bool s) : filename(fname), toShow(s) {
    try {
        std::vector<spdlog::sink_ptr> sinks;
        
        // Create logs directory if it doesn't exist
        std::filesystem::create_directories("logs");
        
        // Daily rotating file sink (rotates at midnight, keeps 10 files)
        auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>("logs/ppos.log", 0, 0, false, 10);
        file_sink->set_level(spdlog::level::trace);
        sinks.push_back(file_sink);
        
        // Console sink if toShow is true or filename is empty
        if (toShow || fname.empty()) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::trace);
            sinks.push_back(console_sink);
        }
        
        // Create multi-sink logger
        spdLogger = std::make_shared<spdlog::logger>("ppos_logger", sinks.begin(), sinks.end());
        spdLogger->set_level(spdlog::level::trace);
        
        // Set pattern to match original format
        if (showThreadID) {
            spdLogger->set_pattern("%Y-%m-%d %H:%M:%S.%f:[%t]: %v");
        } else {
            spdLogger->set_pattern("%Y-%m-%d %H:%M:%S.%f: %v");
        }
        
        spdLogger->flush_on(spdlog::level::info);
        spdlog::register_logger(spdLogger);
        
    } catch (const std::exception &e) {
        std::cout << "Logger initialize error: " << e.what() << std::endl;
    }
}

Logger::~Logger() {
    if (spdLogger) {
        spdLogger->flush();
        spdlog::drop("ppos_logger");
    }
}

std::string Logger::GetThreadID() {
    auto idx = GetThreadIndex(std::this_thread::get_id());
    return fmt::format("{:03d}", idx);
}

std::shared_ptr<Logger> Logger::InitializeLogger(const std::string &fileName, bool toShow) {
    auto l = std::make_shared<Logger>(fileName, toShow);
    return l;
}

void Logger::UnInitializeLogger(std::shared_ptr<Logger> &l) {
    l.reset();
}

extern UniversalUniqueID thisServerID;

static std::string GetUniqueID(const std::string &str, int len = 2) {
    static std::mutex mtx;
    static std::size_t n = 0;
    static std::unordered_map<std::string, std::size_t> stringMap;
    std::lock_guard<std::mutex> lock(mtx);
    auto iter = stringMap.find(str);
    if (iter == stringMap.end()) {
        stringMap[str] = n++;
    }
    return fmt::format("{:0{}d}", stringMap[str], len);
}

std::string Logger::GetTimeInString(std::chrono::system_clock::time_point utcNow) {
    static char buf[100];
    static std::mutex mtx;
    static std::lock_guard<std::mutex> __lock(mtx);

    std::string absTime;
    if (Logger::showAbsTime) {
        absTime = loggerAppCode + "~" + GetUniqueID(thisServerID.GetString()) + "~";
    }
    try {
        auto time = std::chrono::system_clock::to_time_t(utcNow);
        auto tm = std::localtime(&time);

        std::string msfix = "";
        if (toShowMillisecond) {
            auto lt = utcNow.time_since_epoch();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(lt).count() % 1000;
            msfix = fmt::format("{:03}", ms);
        }
        auto len = std::strftime(buf, 100, "%T", tm);
        if (len > 0)
            return fmt::format("{}{}:{}", absTime, buf, msfix);
    }
    catch (const std::exception& e) {
        std::cout << "Logger::GetTimeInString error: " << e.what() << std::endl;
    }
    return absTime;
}

void Logger::Write(const std::string &msg) {
    try {
        if (spdLogger) {
            spdLogger->info(msg);
        } else {
            std::cout << msg << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cout << "Logger::Write error: " << e.what() << std::endl;
    }
}

static bool logThreadWasAlive = false;

std::shared_ptr<Logger> Logger::logger;
std::recursive_mutex Logger::loggerMutex;

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

#if defined(__WXWINDOWS_) || defined(__WXGTK__) || defined(__WXMSW__)
TimeTracker::TimeTracker(const std::string &m) : msg(m) {
    tStart = std::chrono::steady_clock::now();
    auto ts = Logger::GetTimeInString(std::chrono::system_clock::now());
    ShowLog(fmt::format("{}: {} started.", ts, msg));
}
TimeTracker::~TimeTracker() {
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tStart);
    auto timeStr = Logger::GetTimeInString(std::chrono::system_clock::now());
    ShowLog(fmt::format("{}: {} completed. Elapsed {} ms.", timeStr, msg, ts.count()));
}
#endif

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
