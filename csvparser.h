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

/// Read a whole file as UTF-8 text. Handles a UTF-8 BOM, UTF-16 LE/BE with a
/// BOM and (as a safety net) BOM-less UTF-16 that the NUL-byte pattern gives
/// away; anything else is returned byte-for-byte (e.g. Windows-1252 files).
/// Returns false when the file cannot be read; `error` then carries a message.
bool ReadAllText(const std::string& path, std::string& out, std::string* error = nullptr);

/// ReadAllText + Parse. Used by the master-data importer, which must apply the
/// exact same decoding and quoting rules as the setup preview.
Table ParseFile(const std::string& path, char delimiter = 0, bool firstRowIsHeader = true,
                std::string* error = nullptr);

} // namespace CsvParser
