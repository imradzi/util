
#include "md2html.h"

#include <cctype>
#include <sstream>
#include <string>
#include <vector>

std::string md2html(const std::string& md, bool darkMode) {
    auto esc = [](const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                default: out += c;
            }
        }
        return out;
    };

    const char* bgColor = darkMode ? "#1e1e1e" : "#ffffff";
    const char* textColor = darkMode ? "#e0e0e0" : "#000000";
    std::string html = "<html><body bgcolor='" + std::string(bgColor) + "' text='" + std::string(textColor) + "'>";
    html += "<font face='Arial,Helvetica,sans-serif' size=3>";

    std::istringstream in(md);
    std::string line, block;
    bool inCodeBlock = false, inList = false, inOrderedList = false;
    bool inTable = false;
    std::vector<std::vector<std::string>> tableRows;  // buffered rows (header + data)

    auto flushBlock = [&]() {
        if (block.empty()) return;
        html += block;
        block.clear();
    };
    auto closeList = [&](bool& listFlag) {
        if (listFlag) { html += (&listFlag == &inOrderedList) ? "</ol>" : "</ul>"; listFlag = false; }
    };

    // Determine if a cell's content looks numeric (e.g. "~35%", "42", "1,234.56", "RM 500")
    auto isNumericCell = [](const std::string& cell) -> bool {
        std::string s = cell;
        while (!s.empty() && s.front() == ' ') s.erase(0, 1);
        while (!s.empty() && s.back() == ' ') s.pop_back();
        if (s.empty()) return false;
        // Strip leading symbols
        if (s.front() == '~' || s.front() == '-' || s.front() == '+' || s.front() == '$')
            s.erase(0, 1);
        if (s.starts_with("RM ") || s.starts_with("RM")) s.erase(0, s[2] == ' ' ? 3 : 2);
        // Strip trailing %
        if (!s.empty() && s.back() == '%') s.pop_back();
        if (s.empty()) return false;
        bool hasDigit = false;
        for (char c : s) {
            if (std::isdigit(static_cast<unsigned char>(c))) { hasDigit = true; continue; }
            if (c == ',' || c == '.') continue;
            return false;
        }
        return hasDigit;
    };

    // Emit the buffered table with right-aligned numeric columns
    auto emitTable = [&]() {
        if (tableRows.empty()) return;

        // Determine column count (max across all rows)
        size_t ncols = 0;
        for (auto& row : tableRows)
            if (row.size() > ncols) ncols = row.size();

        // Determine which columns are numeric (check all data rows for that column)
        std::vector<bool> colIsNumeric(ncols, true);
        bool hasDataRows = tableRows.size() > 1;  // row 0 = header
        for (size_t c = 0; c < ncols; ++c) {
            bool anyDataInCol = false;
            for (size_t r = 1; r < tableRows.size(); ++r) {
                if (c < tableRows[r].size() && !tableRows[r][c].empty()) {
                    anyDataInCol = true;
                    if (!isNumericCell(tableRows[r][c])) {
                        colIsNumeric[c] = false;
                        break;
                    }
                }
            }
            if (!anyDataInCol) colIsNumeric[c] = false;
        }

        html += "<table border=1 cellpadding=4 cellspacing=0>";
        for (size_t r = 0; r < tableRows.size(); ++r) {
            html += "<tr>";
            bool isHeader = (r == 0);
            for (size_t c = 0; c < ncols; ++c) {
                std::string cell = (c < tableRows[r].size()) ? tableRows[r][c] : "";
                if (isHeader) {
                    html += "<th>";
                } else if (hasDataRows && colIsNumeric[c]) {
                    html += "<td align=right>";
                } else {
                    html += "<td>";
                }
                html += esc(cell);
                html += isHeader ? "</th>" : "</td>";
            }
            html += "</tr>";
        }
        html += "</table>";
        tableRows.clear();
    };

    auto closeTable = [&]() {
        emitTable();
        inTable = false;
    };

    // Helper: split a table row by '|', trim each cell, return vector
    auto splitTableRow = [](const std::string& row) -> std::vector<std::string> {
        std::vector<std::string> cells;
        size_t start = 0;
        // Skip leading '|' if present
        if (!row.empty() && row.front() == '|') start = 1;
        size_t end = start;
        while (end <= row.size()) {
            if (end == row.size() || row[end] == '|') {
                std::string cell = row.substr(start, end - start);
                // Trim
                while (!cell.empty() && cell.front() == ' ') cell.erase(0, 1);
                while (!cell.empty() && cell.back() == ' ') cell.pop_back();
                cells.push_back(cell);
                start = end + 1;
            }
            ++end;
        }
        // Drop trailing empty cell caused by trailing '|' in the row
        if (!cells.empty() && cells.back().empty())
            cells.pop_back();
        return cells;
    };

    // Check if a trimmed row is a table separator (e.g. |---|---| or |:---:|)
    auto isTableSeparator = [](const std::string& trimmed) -> bool {
        if (trimmed.empty() || trimmed.front() != '|') return false;
        for (size_t i = 1; i + 1 < trimmed.size(); ++i) {
            char c = trimmed[i];
            if (c != '-' && c != ':' && c != '|' && c != ' ') return false;
        }
        return trimmed.find('-') != std::string::npos;
    };

    while (std::getline(in, line)) {
        // Code block toggle
        if (line.starts_with("```")) {
            flushBlock();
            closeList(inList); closeList(inOrderedList);
            if (!inCodeBlock) {
                inCodeBlock = true;
                html += "<pre><code>";
            } else {
                html += "</code></pre>";
                inCodeBlock = false;
            }
            continue;
        }
        if (inCodeBlock) {
            html += esc(line) + "\n";
            continue;
        }

        // Blank line — close lists, close table, and start new paragraph
        if (line.empty()) {
            flushBlock();
            closeList(inList); closeList(inOrderedList);
            closeTable();
            if (!block.empty() || !html.ends_with("<p>"))
                html += "<p>";
            continue;
        }

        std::string trimmed = line;
        // Trim leading spaces
        while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(0, 1);

        // Table row detection: line starts with '|' and contains at least one more '|'
        if (trimmed.front() == '|' && trimmed.find('|', 1) != std::string::npos) {
            flushBlock();
            closeList(inList); closeList(inOrderedList);

            if (isTableSeparator(trimmed)) {
                // Separator row: skip (don't buffer)
                inTable = true;  // ensure we stay in table mode
                continue;
            }

            if (!inTable) {
                inTable = true;
                tableRows.clear();
            }

            tableRows.push_back(splitTableRow(trimmed));
            continue;
        }

        // Heading
        if (trimmed.starts_with("###### ")) {
            flushBlock(); closeList(inList); closeList(inOrderedList);
            html += "<h6>" + esc(trimmed.substr(7)) + "</h6>";
            continue;
        }
        if (trimmed.starts_with("##### ")) {
            flushBlock(); closeList(inList); closeList(inOrderedList);
            html += "<h5>" + esc(trimmed.substr(6)) + "</h5>";
            continue;
        }
        if (trimmed.starts_with("#### ")) {
            flushBlock(); closeList(inList); closeList(inOrderedList);
            html += "<h4>" + esc(trimmed.substr(5)) + "</h4>";
            continue;
        }
        if (trimmed.starts_with("### ")) {
            flushBlock(); closeList(inList); closeList(inOrderedList);
            html += "<h3>" + esc(trimmed.substr(4)) + "</h3>";
            continue;
        }
        if (trimmed.starts_with("## ")) {
            flushBlock(); closeList(inList); closeList(inOrderedList);
            html += "<h2>" + esc(trimmed.substr(3)) + "</h2>";
            continue;
        }
        if (trimmed.starts_with("# ")) {
            flushBlock(); closeList(inList); closeList(inOrderedList);
            html += "<h1>" + esc(trimmed.substr(2)) + "</h1>";
            continue;
        }

        // Unordered list
        if ((trimmed.starts_with("- ") || trimmed.starts_with("* ")) && trimmed.size() > 2) {
            flushBlock(); closeList(inOrderedList);
            if (!inList) { html += "<ul>"; inList = true; }
            html += "<li>" + esc(trimmed.substr(2)) + "</li>";
            continue;
        }

        // Ordered list
        if (trimmed.size() > 3 && trimmed[0] >= '0' && trimmed[0] <= '9' && trimmed[1] == '.' && trimmed[2] == ' ') {
            flushBlock(); closeList(inList);
            if (!inOrderedList) { html += "<ol>"; inOrderedList = true; }
            html += "<li>" + esc(trimmed.substr(3)) + "</li>";
            continue;
        }

        // Regular paragraph line
        closeList(inList); closeList(inOrderedList);
        closeTable();
        if (!block.empty()) block += " ";
        block += esc(line);
    }
    flushBlock();
    closeList(inList); closeList(inOrderedList);
    closeTable();
    if (inCodeBlock) html += "</code></pre>";

    html += "</font></body></html>";

    // Post-process for inline formatting (bold, italic, code, links)
    // Run on the concatenated HTML using simple regex-style replacements.
    // Order: code first (to avoid formatting inside code), then bold, italic, links.
    auto replaceInline = [](std::string& s, const std::string& pattern,
                             const std::string& openTag, const std::string& closeTag) {
        size_t pos = 0;
        while ((pos = s.find(pattern, pos)) != std::string::npos) {
            size_t end = s.find(pattern, pos + pattern.size());
            if (end == std::string::npos) break;
            std::string inner = s.substr(pos + pattern.size(), end - pos - pattern.size());
            s.replace(pos, end - pos + pattern.size(), openTag + inner + closeTag);
            pos += openTag.size() + inner.size() + closeTag.size();
        }
    };

    replaceInline(html, "`", "<code>", "</code>");
    replaceInline(html, "**", "<b>", "</b>");
    replaceInline(html, "*", "<i>", "</i>");
    // Links: [text](url)
    {
        size_t pos = 0;
        while ((pos = html.find("[", pos)) != std::string::npos) {
            size_t mid = html.find("](", pos + 1);
            if (mid == std::string::npos) break;
            size_t end = html.find(")", mid + 2);
            if (end == std::string::npos) break;
            std::string text = html.substr(pos + 1, mid - pos - 1);
            std::string url  = html.substr(mid + 2, end - mid - 2);
            std::string anchor = "<a href='" + url + "'>" + text + "</a>";
            html.replace(pos, end - pos + 1, anchor);
            pos += anchor.size();
        }
    }
    return html;
}
