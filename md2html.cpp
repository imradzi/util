
#include "md2html.h"

#include <cctype>
#include <cmark-gfm.h>
#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm-extension_api.h>
#include <string>
#include <vector>

// ── helpers ──────────────────────────────────────────────────────────

static bool isNumericCell(const std::string& cell)
{
    std::string s = cell;
    while (!s.empty() && s.front() == ' ') s.erase(0, 1);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    if (s.empty()) return false;

    if (s.front() == '~' || s.front() == '-' || s.front() == '+' || s.front() == '$')
        s.erase(0, 1);
    if (s.starts_with("RM ") || s.starts_with("RM"))
        s.erase(0, s[2] == ' ' ? 3 : 2);
    if (!s.empty() && s.back() == '%') s.pop_back();
    if (s.empty()) return false;

    bool hasDigit = false;
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c))) { hasDigit = true; continue; }
        if (c == ',' || c == '.') continue;
        return false;
    }
    return hasDigit;
}

// Strip HTML tags from a string (for extracting plain-text cell content)
static std::string stripTags(const std::string& html)
{
    std::string out;
    bool inTag = false;
    for (size_t i = 0; i < html.size(); ++i) {
        if (html[i] == '<') { inTag = true; continue; }
        if (html[i] == '>') { inTag = false; continue; }
        if (!inTag) out += html[i];
    }
    return out;
}

// Post-process: add align=right to <td> in columns where all data cells are numeric
static std::string postProcessTables(const std::string& html)
{
    // Helper: find next <tag...> for "th" or "td", return position of '<'
    // and set out_end to position after '>', set out_inner to position after '>'.
    auto findCellTag = [](const std::string& s, size_t start,
                          std::string& out_tagName, size_t& out_tagEnd) -> size_t {
        size_t tagStart = std::string::npos;
        size_t foundTh = s.find("<th", start);
        size_t foundTd = s.find("<td", start);
        if (foundTh < foundTd) { tagStart = foundTh; out_tagName = "th"; }
        else if (foundTd != std::string::npos) { tagStart = foundTd; out_tagName = "td"; }
        if (tagStart == std::string::npos) return std::string::npos;
        out_tagEnd = s.find('>', tagStart);
        if (out_tagEnd == std::string::npos) return std::string::npos;
        ++out_tagEnd;  // past '>'
        return tagStart;
    };

    auto findCloseTag = [](const std::string& s, const std::string& tagName,
                           size_t start, size_t& out_end) -> size_t {
        std::string closeTag = "</" + tagName + ">";
        size_t pos = s.find(closeTag, start);
        if (pos == std::string::npos) return std::string::npos;
        out_end = pos + closeTag.size();
        return pos;
    };

    std::string result;
    size_t pos = 0;

    while (true) {
        size_t tStart = html.find("<table>", pos);
        if (tStart == std::string::npos) { result += html.substr(pos); break; }
        result += html.substr(pos, tStart - pos);

        size_t tEnd = html.find("</table>", tStart);
        if (tEnd == std::string::npos) { result += html.substr(tStart); break; }
        tEnd += 8;  // past "</table>"

        std::string table = html.substr(tStart, tEnd - tStart);

        // ── collect rows ────────────────────────────────────────
        std::vector<std::string> rows;
        size_t rp = 0;
        while (true) {
            size_t trS = table.find("<tr>", rp);
            if (trS == std::string::npos) break;
            size_t trE = table.find("</tr>", trS);
            if (trE == std::string::npos) break;
            trE += 5;
            rows.push_back(table.substr(trS, trE - trS));
            rp = trE;
        }
        if (rows.size() < 2) { result += table; pos = tEnd; continue; }

        // ── determine column count from header row ──────────────
        size_t ncols = 0;
        {
            size_t cp = 0;
            std::string tagName;
            size_t tagEnd;
            while (findCellTag(rows[0], cp, tagName, tagEnd) != std::string::npos) {
                ++ncols;
                cp = tagEnd;
            }
        }
        if (ncols == 0) { result += table; pos = tEnd; continue; }

        // ── analyse data rows ────────────────────────────────────
        std::vector<bool> colNum(ncols, true);
        std::vector<bool> colHasData(ncols, false);
        for (size_t r = 1; r < rows.size(); ++r) {
            size_t cp = 0, ci = 0;
            while (ci < ncols) {
                std::string tagName;
                size_t tagEnd;
                size_t tagStart = findCellTag(rows[r], cp, tagName, tagEnd);
                if (tagStart == std::string::npos) break;
                if (tagName != "td") { cp = tagEnd; continue; }

                size_t closeEnd;
                size_t closeStart = findCloseTag(rows[r], "td", tagEnd, closeEnd);
                if (closeStart == std::string::npos) break;

                std::string cellHtml = rows[r].substr(tagEnd, closeStart - tagEnd);
                std::string cellText = stripTags(cellHtml);
                if (!cellText.empty()) {
                    colHasData[ci] = true;
                    if (!isNumericCell(cellText)) colNum[ci] = false;
                }
                cp = closeEnd;
                ++ci;
            }
        }
        for (size_t c = 0; c < ncols; ++c)
            if (!colHasData[c]) colNum[c] = false;

        // ── rebuild table with alignment ─────────────────────────
        std::string newTable = "<table border=1 cellpadding=4 cellspacing=0>";
        for (size_t r = 0; r < rows.size(); ++r) {
            newTable += "<tr>";
            bool isHdr = (r == 0);
            size_t cp = 0, ci = 0;
            const char* tag = isHdr ? "th" : "td";
            while (ci < ncols) {
                std::string tagName;
                size_t tagEnd;
                size_t tagStart = findCellTag(rows[r], cp, tagName, tagEnd);
                if (tagStart == std::string::npos) break;

                // Skip non-matching tags (e.g., <th> in a data row if malformed)
                if (tagName != tag) { cp = tagEnd; continue; }

                size_t closeEnd;
                size_t closeStart = findCloseTag(rows[r], tag, tagEnd, closeEnd);
                if (closeStart == std::string::npos) break;

                std::string inner = rows[r].substr(tagEnd, closeStart - tagEnd);
                if (!isHdr && colNum[ci]) {
                    newTable += "<td align=right>" + inner + "</td>";
                } else if (isHdr) {
                    newTable += "<th>" + inner + "</th>";
                } else {
                    newTable += "<td>" + inner + "</td>";
                }
                cp = closeEnd;
                ++ci;
            }
            newTable += "</tr>";
        }
        newTable += "</table>";
        result += newTable;
        pos = tEnd;
    }
    return result;
}


// ── public API


// ── public API ──────────────────────────────────────────────────────

std::string md2html(const std::string& md, bool darkMode)
{
    // 1. Register GFM extensions (tables, strikethrough, autolinks, tasklist)
    static bool extensionsRegistered = false;
    if (!extensionsRegistered) {
        cmark_gfm_core_extensions_ensure_registered();
        extensionsRegistered = true;
    }

    // 2. Create parser, attach table extension, build extensions list
    cmark_parser* parser = cmark_parser_new(CMARK_OPT_DEFAULT);
    cmark_syntax_extension* tableExt = cmark_find_syntax_extension("table");
    if (tableExt)
        cmark_parser_attach_syntax_extension(parser, tableExt);

    // 3. Parse Markdown → AST
    cmark_parser_feed(parser, md.c_str(), md.size());
    cmark_node* doc = cmark_parser_finish(parser);

    // 4. Build extensions list for renderer (use default allocator, not NULL)
    cmark_mem* mem = cmark_get_default_mem_allocator();
    cmark_llist* exts = NULL;
    if (tableExt)
        exts = cmark_llist_append(mem, exts, tableExt);

    // 5. Render AST → HTML
    char* raw = cmark_render_html(doc, CMARK_OPT_DEFAULT, exts);
    std::string body = raw ? raw : "";
    free(raw);

    // 6. Cleanup
    if (exts) cmark_llist_free(mem, exts);
    cmark_node_free(doc);
    cmark_parser_free(parser);

    // 7. Add table borders (cmark-gfm outputs plain <table>)
    {
        size_t p = 0;
        while ((p = body.find("<table>", p)) != std::string::npos) {
            body.replace(p, 7, "<table border=1 cellpadding=4 cellspacing=0>");
            p += 42;  // length of replacement string
        }
    }

    // 8. Right-align numeric table columns
    body = postProcessTables(body);

    // 9. Wrap with dark / light theme
    const char* bg = darkMode ? "#1e1e1e" : "#ffffff";
    const char* fg = darkMode ? "#e0e0e0" : "#000000";
    return "<html><body bgcolor='" + std::string(bg) + "' text='" + std::string(fg) + "'>"
           + body + "</body></html>";
}
