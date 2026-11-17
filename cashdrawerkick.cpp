#include "cashdrawerkick.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winspool.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

std::string Trim(const std::string& value) {
    size_t first = 0;
    size_t last = value.size();
    while (first < last && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
    return value.substr(first, last - first);
}

std::string ToUpperAscii(std::string value) {
    for (auto& ch : value) {
        if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    }
    return value;
}

/// "LPT2:", "COM10", "lpt1" - a DOS port. The digit requirement keeps a
/// printer model such as "Compaq LaserJet" from looking like COM1.
bool IsDosPort(const std::string& upper) {
    if (upper.compare(0, 3, "LPT") != 0 && upper.compare(0, 3, "COM") != 0) return false;
    return upper.size() > 3 && std::isdigit(static_cast<unsigned char>(upper[3])) != 0;
}

#ifdef _WIN32

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return std::wstring();
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                           static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) return std::wstring();
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(),
                        length);
    return wide;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return std::string();
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                           static_cast<int>(text.size()), nullptr, 0, nullptr,
                                           nullptr);
    if (length <= 0) return std::string();
    std::string utf8(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(),
                        length, nullptr, nullptr);
    return utf8;
}

/// "Access is denied. (error 5)" - readable in the client log and in the setup
/// dialog without pulling in the setup-only FormatWin32Error().
std::string Win32ErrorMessage(DWORD error) {
    wchar_t* message = nullptr;
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                            FORMAT_MESSAGE_FROM_SYSTEM |
                                            FORMAT_MESSAGE_IGNORE_INSERTS,
                                        nullptr, error, 0,
                                        reinterpret_cast<LPWSTR>(&message), 0, nullptr);
    std::wstring text;
    if (length && message) {
        text.assign(message, length);
        while (!text.empty() &&
               (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' ')) {
            text.pop_back();
        }
    }
    if (message) LocalFree(message);
    if (text.empty()) return "error " + std::to_string(error);
    return WideToUtf8(text) + " (error " + std::to_string(error) + ")";
}

/// Status bits that mean "this job would wait in the queue and kick the drawer
/// later". BUSY/PRINTING/IO_ACTIVE are deliberately not here: they are normal.
constexpr DWORD kBlockingPrinterStatus = PRINTER_STATUS_PAUSED | PRINTER_STATUS_ERROR |
                                         PRINTER_STATUS_PAPER_JAM | PRINTER_STATUS_PAPER_OUT |
                                         PRINTER_STATUS_PAPER_PROBLEM | PRINTER_STATUS_OFFLINE |
                                         PRINTER_STATUS_NOT_AVAILABLE |
                                         PRINTER_STATUS_USER_INTERVENTION |
                                         PRINTER_STATUS_DOOR_OPEN;

#endif  // _WIN32

} // namespace

namespace CashDrawerKick {

bool IsSpoolDevice(const std::string& device) {
    const std::string text = Trim(device);
    if (text.empty()) return false;
    // "\\.\LPT1" is the Win32 device namespace: still a port.
    if (text.compare(0, 4, "\\\\.\\") == 0) return false;
    // "\\server\printer" is a remote queue.
    if (text.size() >= 2 && text[0] == '\\' && text[1] == '\\') return true;
    const std::string upper = ToUpperAscii(text);
    if (IsDosPort(upper)) return false;
    if (upper.compare(0, 5, "/DEV/") == 0) return false;
    return true;
}

std::string BuildKickBytes(const std::string& command, int pin, int pulseMs) {
    const std::string upper = ToUpperAscii(Trim(command));
    if (!upper.empty() && upper != "ESC/POS") return command;  // drawer-specific text

    const int safePin = pin == 1 ? 1 : 0;  // 0 = drawer 1 / pin 2, 1 = drawer 2 / pin 5
    int pulse = pulseMs;
    if (pulse < 20) pulse = 20;
    if (pulse > 500) pulse = 500;

    std::string bytes;
    bytes.reserve(5);
    bytes.push_back('\x1B');                     // ESC p m t1 t2
    bytes.push_back('p');
    bytes.push_back(static_cast<char>(safePin));
    bytes.push_back(static_cast<char>(pulse / 2));  // t1: 2 ms units
    bytes.push_back(static_cast<char>(50));         // t2: 100 ms off time
    return bytes;
}

bool CanOpenPortDevice(const std::string& device) {
    const std::string text = Trim(device);
    if (text.empty() || IsSpoolDevice(text)) return false;
#ifdef _WIN32
    const std::wstring wide = Utf8ToWide(text);
    HANDLE handle =
        CreateFileW(wide.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    CloseHandle(handle);
    return true;
#else
    const int fd = ::open(text.c_str(), O_WRONLY | O_NOCTTY);
    if (fd < 0) return false;
    ::close(fd);
    return true;
#endif
}

bool SpoolRaw(const std::string& queue, const std::string& bytes, std::string& error) {
    error.clear();
#ifdef _WIN32
    if (queue.empty()) {
        error = "no printer queue is configured for the cash drawer";
        return false;
    }
    if (bytes.empty()) {
        error = "the cash drawer command is empty";
        return false;
    }

    const std::wstring name = Utf8ToWide(queue);
    HANDLE printer = nullptr;
    if (!OpenPrinterW(const_cast<LPWSTR>(name.c_str()), &printer, nullptr)) {
        error = "cannot reach printer '" + queue + "': " + Win32ErrorMessage(GetLastError());
        return false;
    }

    // Refuse while the printer is offline / paused: the spooler would keep the
    // job and fire it when the printer comes back, opening the drawer at a
    // random later moment.
    DWORD needed = 0;
    GetPrinterW(printer, 2, nullptr, 0, &needed);
    if (needed > 0) {
        std::vector<unsigned char> buffer(needed);
        if (GetPrinterW(printer, 2, buffer.data(), needed, &needed)) {
            const auto* info = reinterpret_cast<const PRINTER_INFO_2W*>(buffer.data());
            if ((info->Status & kBlockingPrinterStatus) != 0) {
                char status[16] = {};
                std::snprintf(status, sizeof(status), "%08lX",
                              static_cast<unsigned long>(info->Status));
                error = "printer '" + queue + "' is not ready (status 0x" + status +
                        ") - the cash drawer was not kicked";
                ClosePrinter(printer);
                return false;
            }
        }
    }

    DOC_INFO_1W doc{};
    doc.pDocName = const_cast<LPWSTR>(L"PharmaPOS cash drawer");
    doc.pDatatype = const_cast<LPWSTR>(L"RAW");  // never routed through the driver

    bool ok = false;
    if (!StartDocPrinterW(printer, 1, reinterpret_cast<LPBYTE>(&doc))) {
        error = "printer '" + queue + "' refused a raw job: " + Win32ErrorMessage(GetLastError());
    } else {
        DWORD written = 0;
        if (WritePrinter(printer, const_cast<char*>(bytes.data()),
                         static_cast<DWORD>(bytes.size()), &written) &&
            written == bytes.size()) {
            ok = true;
        } else {
            error = "cannot send the drawer command to '" + queue + "': " +
                    Win32ErrorMessage(GetLastError());
        }
        EndDocPrinter(printer);
    }
    ClosePrinter(printer);
    return ok;
#else
    (void)queue;
    (void)bytes;
    error = "a raw drawer job needs Windows; point the device at a /dev/... port instead";
    return false;
#endif
}

} // namespace CashDrawerKick
