#include "csvparser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace CsvParser {

namespace {

std::string StripBom(const std::string& value) {
    if (value.size() >= 3 &&
        static_cast<unsigned char>(value[0]) == 0xEF &&
        static_cast<unsigned char>(value[1]) == 0xBB &&
        static_cast<unsigned char>(value[2]) == 0xBF) {
        return value.substr(3);
    }
    return value;
}

void AppendUtf8(std::string& out, unsigned int codePoint) {
    if (codePoint < 0x80) {
        out.push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

std::string Utf16ToUtf8(const std::string& bytes, size_t offset, bool bigEndian) {
    std::string out;
    out.reserve(bytes.size());
    auto unitAt = [&](size_t index) -> unsigned int {
        const auto lo = static_cast<unsigned char>(bytes[index]);
        const auto hi = static_cast<unsigned char>(bytes[index + 1]);
        return bigEndian ? (static_cast<unsigned int>(lo) << 8) | hi
                         : (static_cast<unsigned int>(hi) << 8) | lo;
    };
    for (size_t i = offset; i + 1 < bytes.size(); i += 2) {
        unsigned int unit = unitAt(i);
        if (unit >= 0xD800 && unit <= 0xDBFF && i + 3 < bytes.size()) {
            const unsigned int low = unitAt(i + 2);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                AppendUtf8(out, 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00));
                i += 2;
                continue;
            }
        }
        if (unit == 0xFEFF) continue;  // stray BOM
        AppendUtf8(out, unit);
    }
    return out;
}

/// BOM-less UTF-16 shows up as every other byte being NUL for ASCII text.
bool LooksLikeUtf16(const std::string& bytes, bool& bigEndian) {
    const size_t limit = std::min<size_t>(bytes.size(), 4096);
    if (limit < 4) return false;
    size_t evenNul = 0, oddNul = 0;
    for (size_t i = 0; i < limit; ++i) {
        if (bytes[i] != '\0') continue;
        if (i % 2 == 0) ++evenNul;
        else ++oddNul;
    }
    const size_t pairs = limit / 2;
    if (oddNul > pairs / 2 && evenNul < pairs / 8) {  // "a\0b\0" = UTF-16LE
        bigEndian = false;
        return true;
    }
    if (evenNul > pairs / 2 && oddNul < pairs / 8) {  // "\0a\0b" = UTF-16BE
        bigEndian = true;
        return true;
    }
    return false;
}

} // namespace

char DetectDelimiter(const std::string& firstLine) {
    int counts[4] = {0, 0, 0, 0};  // comma ; tab |
    bool inQuotes = false;
    for (char c : firstLine) {
        if (c == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (inQuotes) continue;
        if (c == ',') ++counts[0];
        else if (c == ';') ++counts[1];
        else if (c == '\t') ++counts[2];
        else if (c == '|') ++counts[3];
    }
    int best = 0;
    for (int i = 1; i < 4; ++i)
        if (counts[i] > counts[best]) best = i;
    static const char delimiters[4] = {',', ';', '\t', '|'};
    return counts[best] > 0 ? delimiters[best] : ',';
}

std::vector<std::string> ParseLine(const std::string& line, char delimiter) {
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field.push_back(c);
            }
        } else if (c == '"') {
            inQuotes = true;
        } else if (c == delimiter) {
            fields.push_back(field);
            field.clear();
        } else {
            field.push_back(c);
        }
    }
    fields.push_back(field);
    return fields;
}

Table Parse(const std::string& content, char delimiter, bool firstRowIsHeader) {
    Table table;
    const std::string text = StripBom(content);
    if (text.empty()) return table;

    if (delimiter == 0) {
        const size_t firstBreak = text.find_first_of("\r\n");
        delimiter = DetectDelimiter(text.substr(0, firstBreak));
    }
    table.delimiter = delimiter;
    table.hasHeader = firstRowIsHeader;

    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string field;
    bool inQuotes = false;

    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field.push_back(c);
            }
            continue;
        }
        if (c == '"') {
            inQuotes = true;
        } else if (c == delimiter) {
            row.push_back(field);
            field.clear();
        } else if (c == '\n') {
            row.push_back(field);
            field.clear();
            rows.push_back(row);
            row.clear();
        } else if (c == '\r') {
            // Consume CRLF as one newline; a lone CR is also a newline.
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
            row.push_back(field);
            field.clear();
            rows.push_back(row);
            row.clear();
        } else {
            field.push_back(c);
        }
    }
    if (!field.empty() || !row.empty()) {
        row.push_back(field);
        rows.push_back(row);
    }

    // Drop trailing empty rows produced by a final newline.
    while (!rows.empty()) {
        bool allEmpty = true;
        for (const auto& cell : rows.back())
            if (!cell.empty()) { allEmpty = false; break; }
        if (!allEmpty) break;
        rows.pop_back();
    }
    if (rows.empty()) return table;

    if (firstRowIsHeader) {
        table.headers = rows.front();
        rows.erase(rows.begin());
    } else {
        table.headers.reserve(rows.front().size());
        for (size_t i = 0; i < rows.front().size(); ++i)
            table.headers.push_back("column" + std::to_string(i + 1));
    }
    table.rows = std::move(rows);
    return table;
}

bool ReadAllText(const std::string& path, std::string& out, std::string* error) {
    out.clear();
#ifdef _WIN32
    // The path is UTF-8 (as written into the import spec by ppSetup). Opening a
    // char* path would use the ANSI code page, which breaks any accented or
    // non-Latin folder name, so hand std::ifstream a wide path instead.
    std::wstring widePath;
    {
        const int length =
            MultiByteToWideChar(CP_UTF8, 0, path.data(), static_cast<int>(path.size()), nullptr, 0);
        if (length > 0) {
            widePath.resize(static_cast<size_t>(length));
            MultiByteToWideChar(CP_UTF8, 0, path.data(), static_cast<int>(path.size()),
                                widePath.data(), length);
        }
    }
    std::ifstream file(widePath.empty() ? std::filesystem::path(path) : std::filesystem::path(widePath),
                       std::ios::binary);
#else
    std::ifstream file(path, std::ios::binary);
#endif
    if (!file) {
        if (error) *error = "cannot open " + path;
        return false;
    }
    std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (file.bad()) {
        if (error) *error = "cannot read " + path;
        return false;
    }

    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        out = bytes.substr(3);  // UTF-8 with BOM
        return true;
    }
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        out = Utf16ToUtf8(bytes, 2, false);  // UTF-16 LE
        return true;
    }
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFE &&
        static_cast<unsigned char>(bytes[1]) == 0xFF) {
        out = Utf16ToUtf8(bytes, 2, true);  // UTF-16 BE
        return true;
    }
    bool bigEndian = false;
    if (LooksLikeUtf16(bytes, bigEndian)) {
        out = Utf16ToUtf8(bytes, 0, bigEndian);
        return true;
    }
    out = std::move(bytes);
    return true;
}

Table ParseFile(const std::string& path, char delimiter, bool firstRowIsHeader,
                std::string* error) {
    std::string content;
    if (!ReadAllText(path, content, error)) return Table{};
    return Parse(content, delimiter, firstRowIsHeader);
}

} // namespace CsvParser
