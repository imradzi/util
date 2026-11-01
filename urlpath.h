#pragma once

#include <string>
#include <string_view>

/// URL path helpers.
namespace UrlPath {

/// Percent-decode one path segment.
///
/// Unlike query/form decoding, '+' is a literal character in a path and is
/// left untouched. Invalid escapes (a '%' without two hex digits) are copied
/// through unchanged.
std::string DecodeSegment(std::string_view segment);

} // namespace UrlPath
