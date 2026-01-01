#pragma once
#include <memory>
#include <sstream>
#include <chrono>
#include <iostream>
#include "../wpSQL/include/logging.hpp"

extern std::string loggerAppCode;

// Adapter class to maintain backward compatibility with existing Logger API
class Logger {
    static std::size_t GetThreadIndex(const std::thread::id id);   
public:
    static const char *timeFormat, *dateFormat;
    static bool toShowMillisecond;
    static bool showAbsTime;
    static bool showThreadID;

    static std::shared_ptr<Logger> Initialize(const std::string &fileName) {
        DB::Logger::initialize("ppos", "trace");
        return nullptr;  // Return nullptr as DB::Logger is static
    }
    static std::shared_ptr<Logger> Initialize(bool toShow=false) {
        DB::Logger::initialize("ppos", "trace");
        return nullptr;  // Return nullptr as DB::Logger is static
    }

    static void UnInitialize() {
        // DB::Logger uses RAII, no explicit cleanup needed
    }
    static bool IsLogRunning() { return DB::Logger::get() != nullptr; }
    static void logMessage(const std::string &m) {
        DB::Logger::info(m);
    }
    static bool IsLogActive() { 
        return DB::Logger::get() != nullptr; 
    }
    static std::string GetThreadID();
};

inline void wpLogMessage(const std::string &s) { Logger::logMessage(s); }

class LoggerTracker {
    std::chrono::steady_clock::time_point tStart;
    std::string msg;

public:
    LoggerTracker(const std::string &m, bool showOnlyEnding = true);
    ~LoggerTracker();
};

class LoggerTrackerStreamOut {
    std::chrono::steady_clock::time_point tStart;
    std::string msg;

public:
    LoggerTrackerStreamOut(const std::string &m, bool showOnlyEnding = true);
    ~LoggerTrackerStreamOut();
};

#ifdef __WXWINDOWS__
class TimeTracker {
    std::chrono::steady_clock::time_point tStart;
    std::string msg;

public:
    TimeTracker(const std::string &m);
    ~TimeTracker();
};
#endif

class TimeTrackerStream {
    std::chrono::steady_clock::time_point tStart;
    std::string msg;
    std::ostream &out;
    bool reportInMilliseconds;

public:
    TimeTrackerStream(const std::string &m, std::ostream &x, bool repInms = true);
    void Write(const std::string &s);
    ~TimeTrackerStream();
};

extern bool ShowLog(const std::string &t);

#ifdef __WXWINDOWS__
void wpAssertHandler(const wxString &file, int line, const wxString &func, const wxString &cond, const wxString &msg);
inline void wpDirectAssertMessagesToLogger() {
    wxSetAssertHandler(wpAssertHandler);
}
#endif

inline bool ShowLogOnVerboseDetail(const std::string &str) {
    ShowLog(str);
    return true;
}

inline std::unique_ptr<LoggerTrackerStreamOut> ShowTrackerTest(const std::string &str, bool showOnlyAtEnd = false) {
    return std::make_unique<LoggerTrackerStreamOut>(str, showOnlyAtEnd);
}

inline std::unique_ptr<LoggerTracker> ShowTrackkerOnVerboseDetail(const std::string &str, bool showOnlyAtEnd = false) {
    return std::make_unique<LoggerTracker>(str, showOnlyAtEnd);
}

inline std::unique_ptr<LoggerTracker> ShowTrackkerOnVerbose(const std::string &str, bool showOnlyAtEnd = false) {
    return std::make_unique<LoggerTracker>(str, showOnlyAtEnd);
}

inline std::unique_ptr<LoggerTracker> ShowTrackker(const std::string &str, bool showOnlyAtEnd = false) {
    return std::make_unique<LoggerTracker>(str, showOnlyAtEnd);
}

class AllocationTracker {
    size_t allocated = 0;
    size_t deallocated = 0;
    size_t max = 0;

public:
    void AddMemory(size_t s) {
        allocated += s;
        max = std::max(max, allocated - deallocated);
    }
    void ReleaseMemory(size_t s) {
        deallocated += s;
        max = std::max(max, allocated - deallocated);
    }
    long long GetMemoryUsage() { return (long long)allocated - (long long)deallocated; }
    void Summarize(bool all=false);
};
extern AllocationTracker gAllocationTracker;
