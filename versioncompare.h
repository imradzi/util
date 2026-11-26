#pragma once
// Shared comparison of the "PharmaPOS <role> Ver 3.092 Build 979 Compiled on ..."
// string that both ppServer and ppClient embed (app/version.h via
// wpObject#GetSoftwareVersion).
//
// Two callers need it:
//   • ppServer CreateSession decides `needUpdate` (is the connecting client older?)
//   • ppClient decides whether the server it is connected to runs an older build
//     than the terminal itself (server-outdated banner).
//
// The manifest version in installer.pharmapos.com/manifest.json is a different
// format ("3.092.979") and is compared by appServer/UpdateVersion.h. Keep this
// header free of wxWidgets and of both target-private include paths
// (appServer/ is server-only, appGUI/ is client-only) so it can live in the
// shared util/ layer and be unit-tested on its own.

#include <cctype>
#include <charconv>
#include <string>
#include <string_view>
#include <system_error>

namespace PPOS::VersionCompare {

struct AppVersion {
    std::string versionText;  ///< version token exactly as printed, e.g. "3.092"
    double version {0.0};
    int build {0};
};

namespace detail {

inline bool IsSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

inline bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

inline bool IsDigits(std::string_view s) {
    if (s.empty()) return false;
    for (const char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c)) == 0) return false;
    }
    return true;
}

/// "3", "3.092", "3.092.1" - digits separated by exactly one dot.
inline bool IsDottedNumber(std::string_view s) {
    if (s.empty()) return false;
    bool digitSeen = false;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '.') {
            if (!digitSeen) return false;            // leading dot or ".."
            if (i + 1 == s.size()) return false;     // trailing dot
            digitSeen = false;
        } else if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
            digitSeen = true;
        } else {
            return false;
        }
    }
    return digitSeen;
}

/// Tokenizes on whitespace and returns the token that follows \p tag.
/// Returns an empty view when the tag or its value is missing.
inline std::string_view TokenAfter(std::string_view text, std::string_view tag) {
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && IsSpace(text[i])) ++i;
        const size_t start = i;
        while (i < text.size() && !IsSpace(text[i])) ++i;
        if (start == i) break;
        if (!EqualsIgnoreCase(text.substr(start, i - start), tag)) continue;
        while (i < text.size() && IsSpace(text[i])) ++i;
        const size_t valueStart = i;
        while (i < text.size() && !IsSpace(text[i])) ++i;
        return text.substr(valueStart, i - valueStart);
    }
    return {};
}

}  // namespace detail

/// Parses "PharmaPOS Server Ver 3.092 Build 979 Compiled on 2026-11-27 @ 08:00:00".
/// Accepts any product-name prefix (the customized build name keeps the same
/// tail) and the old multi-line form
/// "PharmaPOS Client\nVer 1.52 Build 85 [1088b41132]\nCompiled on ...".
/// Returns false when "Ver <number>" or "Build <number>" is missing.
inline bool Parse(std::string_view text, AppVersion& out) {
    out = AppVersion {};

    const std::string_view versionToken = detail::TokenAfter(text, "Ver");
    if (!detail::IsDottedNumber(versionToken)) return false;

    const std::string_view buildToken = detail::TokenAfter(text, "Build");
    if (!detail::IsDigits(buildToken)) return false;

    int build = 0;
    const auto [ptr, ec] =
        std::from_chars(buildToken.data(), buildToken.data() + buildToken.size(), build);
    if (ec != std::errc {} || ptr != buildToken.data() + buildToken.size()) return false;

    double version = 0.0;
    try {
        size_t consumed = 0;
        version = std::stod(std::string(versionToken), &consumed);
        if (consumed != versionToken.size()) return false;
    } catch (...) {
        return false;
    }

    out.versionText = std::string(versionToken);
    out.version = version;
    out.build = build;
    return true;
}

/// -1 when \p a is older, 0 when equal, +1 when newer. Version number first,
/// build number second (a build number only exists with three segments, see
/// app/version.h: "3.092" + "." + "979").
inline int Compare(const AppVersion& a, const AppVersion& b) {
    if (a.version != b.version) return a.version < b.version ? -1 : 1;
    if (a.build != b.build) return a.build < b.build ? -1 : 1;
    return 0;
}

/// True only when both strings parse AND \p candidate is strictly older than
/// \p reference. Unparsable input is never "older" - a custom build string must
/// not make the client nag about the server.
inline bool IsOlder(std::string_view candidate, std::string_view reference) {
    AppVersion candidateVersion;
    AppVersion referenceVersion;
    if (!Parse(candidate, candidateVersion) || !Parse(reference, referenceVersion)) return false;
    return Compare(candidateVersion, referenceVersion) < 0;
}

/// "3.092.979" - the shape installer-manifest.json uses, for banners/logs.
inline std::string Format(const AppVersion& value) {
    return value.versionText + "." + std::to_string(value.build);
}

}  // namespace PPOS::VersionCompare
