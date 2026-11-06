#pragma once

#include <string>
#include <vector>

/// Minimal RFC-4180 CSV reader used by the payment settlement importer.
/// Deliberately dependency-free so the parsing rules can be unit-tested
/// without a database or a gateway server.
namespace CsvParser {

struct Table {
    char delimiter = ',';
    bool hasHeader = true;
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
};

/// Pick the most frequent delimiter in a header line. Returns ',' when the
/// line contains none of comma / semicolon / tab / pipe.
char DetectDelimiter(const std::string& firstLine);

/// Parse one logical line (no embedded newlines). Kept public for tests.
std::vector<std::string> ParseLine(const std::string& line, char delimiter);

/// Parse a whole document. A leading UTF-8 BOM is removed. `delimiter == 0`
/// auto-detects from the first line. Quoted fields may contain delimiters and
/// newlines; a doubled quote inside a quoted field is one literal quote.
Table Parse(const std::string& content, char delimiter = 0,
            bool firstRowIsHeader = true);

} // namespace CsvParser
