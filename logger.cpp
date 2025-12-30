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
Logger::Logger(const std::string &fname, bool s) : filename(fname), toShow(s) {  //, mtx(), cond(mtx) {
    nRec = 0;
    try {
        runningThread = std::thread(&Logger::Entry, this);
    } catch (const std::exception &e) {
        std::cout << "Logger initialize creating thread error: " << e.what() << std::endl;
    }
}

Logger::~Logger() {
    if (runningThread.joinable()) {
        messageQueue.send(""); // last message to wakeup the wait.
        messageQueue.close();
        runningThread.join();
    }
}

std::string Logger::GetThreadID() {
    auto idx = GetThreadIndex(std::this_thread::get_id());
    return fmt::format("{:03d}", idx);
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

namespace {
    constexpr auto InvalidTimePoint() { return std::chrono::system_clock::time_point::min(); }
};

static std::string showDate(std::chrono::system_clock::time_point timePoint=std::chrono::system_clock::now()) {
    static char buf[100];
    static std::mutex mtx;
    static std::lock_guard<std::mutex> __lock(mtx);
    auto time = std::chrono::system_clock::to_time_t(timePoint);
    auto tm = std::localtime(&time);
    if (std::strftime(buf, 100, "%Y-%m-%d %a (%W)", tm)) return buf;
    return "";
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
    nRec++;
    try {
        if (runningThread.joinable()) {
            if (showThreadID)
                messageQueue.send(fmt::format("{}:[{}]: {}", GetNowInString(), GetThreadID(), msg));
            else
                messageQueue.send(fmt::format("{}: {}", GetNowInString(), msg));
        } else {
            std::cout << fmt::format("{}:[{}]: {}", GetNowInString(), GetThreadID(), msg) << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cout << "Logger::Write error: " << e.what() << std::endl;
    }
}

extern std::string GetEpoch();
static bool logThreadWasAlive = false;

static void KeepOnlyLatestFiles(const std::string folderName, int maxNoOfFiles) {
    try {
        namespace fs = std::filesystem;
        std::vector<std::string> filenameList;
        for (auto &e : fs::directory_iterator {folderName}) {
            if (e.is_regular_file()) {
                std::string filename = e.path().string();
                filenameList.emplace_back(filename);
            }
        }
        ShowLog(fmt::format("KeepOnlyLatestFiles -> no of files: {}", filenameList.size()));
        std::sort(filenameList.begin(), filenameList.end());
        int cnt = 0;
        for (std::vector<std::string>::reverse_iterator it = filenameList.rbegin(); it != filenameList.rend(); it++) {  // cannot use reverse range
            auto &fname = *it;
            if (++cnt > maxNoOfFiles) {
                if (fs::remove(fname)) {
                    ShowLog(fmt::format("{} removed ", fname));
                }
            } else {
                ShowLog(fmt::format("{} kept ", fname));
            }
        }
    }
    catch (const std::exception& e) {
        std::cout << "KeepOnlyLatestFiles: error: " << e.what() << std::endl;        
    }
}

void Logger::Entry() {
    constexpr int noOfLogsToKeep = 10;
    auto createNewLog = [&]() {
        auto f = fmt::format("logs{}{}.log", std::string(1, std::filesystem::path::preferred_separator), GetEpoch());
        CreateNonExistingFolders(f);
        KeepOnlyLatestFiles("logs", noOfLogsToKeep);
        return std::ofstream(std::string(f));
    };

    constexpr std::chrono::milliseconds WAITING_TIME {500};

    logThreadWasAlive = true;
    std::ostream *out = &std::cout;
    if (toShow) {
        if (!filename.empty()) {
            out = new std::ofstream(filename);
        }
    } else
        out = nullptr;

    auto forcedLog = createNewLog();
    const auto invalidYMD = std::chrono::year_month_day(std::chrono::year(0) / std::chrono::month(0) / std::chrono::day(0));
    long actualWritten = 0;
    auto lastFlush = std::chrono::steady_clock::now();
    try {
        auto prevYMD = invalidYMD;
        const std::chrono::time_zone *tz = nullptr;
        try {
            tz = std::chrono::current_zone();
        } catch (...) {}

        while (!messageQueue.isClosed()) {
            std::string msg;
            std::this_thread::yield();
            auto rc = messageQueue.receive(msg);
            if (rc == MQ::OK) {
                auto now = tz ? tz->to_local(std::chrono::system_clock::now()) : std::chrono::local_time<std::chrono::system_clock::duration>(std::chrono::system_clock::now().time_since_epoch());  // localnow...
                auto ymdNow = std::chrono::year_month_day(std::chrono::floor<std::chrono::days>(now));
                if (prevYMD == invalidYMD) {
                    if (out) *out << "---------------" << showDate() << "---------------" << std::endl;
                    forcedLog << "---------------" << showDate() << "---------------" << std::endl;
                } else {
                    if (ymdNow != prevYMD) {
                        if (out) *out << "---------------" << showDate() << "---------------" << std::endl;
                        forcedLog << "---------------" << showDate() << "---------------" << std::endl;
                        forcedLog << std::flush;
                        forcedLog.close();
                        forcedLog = createNewLog();
                        forcedLog << "---------------" << showDate() << "---------------" << std::endl;
                    }
                };
                prevYMD = ymdNow;

                actualWritten++;

                if (out) *out << msg << std::endl;
                forcedLog << msg << std::endl;
            } else if (rc == MQ::Closed) {
                if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastFlush).count() > 500) {
                    if (out) out->flush();
                    forcedLog.flush();
                    lastFlush = std::chrono::steady_clock::now();
                }
            }
        }
    } catch (std::runtime_error &s) {
        if (out) *out << GetNowInString() << ": " << s.what() << std::endl;
        forcedLog << GetNowInString() << ": " << s.what() << std::endl;
    } catch (...) {
        if (out) *out << GetNowInString() << ": Unknown exception!" << std::endl;
        forcedLog << GetNowInString() << ": Unknown exception!" << std::endl;
    }

    if (out) {
        *out << "No of records written=" << actualWritten << " Write called=" << nRec << std::endl;
        *out << "logger closed." << std::endl;
        *out << std::flush;
    }
    forcedLog << "No of records written=" << actualWritten << " Write called=" << nRec << std::endl;
    forcedLog << "logger closed." << std::endl;
    forcedLog << std::flush;

    if (!filename.empty()) delete out;
}

std::shared_ptr<Logger> Logger::logger;
std::recursive_mutex Logger::loggerMutex;

bool ShowLog(const std::string &t) {
    if (Logger::IsLogActive()) {
        Logger::logMessage(t);
        return true;
    } else {
        std::cout << t << std::endl;
    }
    if (logThreadWasAlive) {  // if log was alive and missed the thread.
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
