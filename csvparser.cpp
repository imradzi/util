#include "csvparser.h"

#include <algorithm>

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

} // namespace CsvParser
