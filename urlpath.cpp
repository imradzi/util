#include "urlpath.h"

#include <cctype>

namespace UrlPath {

std::string DecodeSegment(std::string_view segment) {
    std::string out;
    out.reserve(segment.size());
    for (size_t i = 0; i < segment.size(); ++i) {
        const char c = segment[i];
        if (c == '%' && i + 2 < segment.size() &&
            std::isxdigit(static_cast<unsigned char>(segment[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(segment[i + 2]))) {
            auto hexVal = [](char h) -> int {
                if (h >= '0' && h <= '9') return h - '0';
                if (h >= 'a' && h <= 'f') return h - 'a' + 10;
                return h - 'A' + 10;
            };
            out.push_back(static_cast<char>((hexVal(segment[i + 1]) << 4) |
                                            hexVal(segment[i + 2])));
            i += 2;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

} // namespace UrlPath
