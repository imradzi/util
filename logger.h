#pragma once
#include <memory>
#include <sstream>
#include <chrono>
#include <mutex>
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

extern std::string loggerAppCode;

class Logger {
    static std::shared_ptr<Logger> InitializeLogger(const std::string &fileName, bool toShow);
    static void UnInitializeLogger(std::shared_ptr<Logger> &l);

    std::string filename;
    static std::shared_ptr<Logger> logger;
    static std::recursive_mutex loggerMutex;
    std::shared_ptr<spdlog::logger> spdLogger;
    bool toShow {false};

public:
    Logger(const std::string &fname, bool toShow);
    ~Logger();
    static const char *timeFormat, *dateFormat;
    static bool toShowMillisecond;
    static bool showAbsTime;
    static bool showThreadID;

    static std::string GetNowInString() {
        return GetTimeInString(std::chrono::system_clock::now());
    }
    static std::string GetTimeInString(std::chrono::system_clock::time_point now);

    void Write(const std::string &msg);
    
    static std::shared_ptr<Logger> Initialize(const std::string &fileName) {
        std::lock_guard _lock(loggerMutex);
        return logger = InitializeLogger(fileName, true); 
    }
    static std::shared_ptr<Logger> Initialize(bool toShow=false) {
        std::lock_guard _lock(loggerMutex);
        return logger = InitializeLogger("", toShow); 
    }

    static void UnInitialize() { 
        std::lock_guard _lock(loggerMutex);
        UnInitializeLogger(logger); 
    }
    static bool IsLogRunning() { return logger != nullptr; }
    static void logMessage(const std::string &m) {
        std::lock_guard _lock(loggerMutex);
        if (logger) {
            logger->Write(m);
        } else
            std::cout << m << std::endl;
    }
    static bool IsLogActive() { 
        std::lock_guard _lock(loggerMutex);
        return logger != nullptr; 
    }
    static std::string GetThreadID();
};

inline void wpLogMessage(const std::string &s) { Logger::logMessage(s); }

//inline void StartMessageLogger(const std::string &fileName) {
//	logger = new Logger(fileName);
//	logger->Run();
//}

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
