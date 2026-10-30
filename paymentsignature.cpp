#include "paymentsignature.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace {

std::string LowerCopy(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

std::string TrimCopy(const std::string& value) {
    size_t begin = 0;
    size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(begin, end - begin);
}

std::string UrlDecode(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if (c == '+') {
            out.push_back(' ');
        } else if (c == '%' && i + 2 < value.size() &&
                   std::isxdigit(static_cast<unsigned char>(value[i + 1])) &&
                   std::isxdigit(static_cast<unsigned char>(value[i + 2]))) {
            auto hexVal = [](char h) -> int {
                if (h >= '0' && h <= '9') return h - '0';
                if (h >= 'a' && h <= 'f') return h - 'a' + 10;
                return h - 'A' + 10;
            };
            out.push_back(static_cast<char>((hexVal(value[i + 1]) << 4) | hexVal(value[i + 2])));
            i += 2;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

void ParseUrlEncodedInto(const std::string& body, std::map<std::string, std::string>& out) {
    size_t start = 0;
    while (start <= body.size()) {
        size_t amp = body.find('&', start);
        std::string pair = body.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
        if (!pair.empty()) {
            size_t eq = pair.find('=');
            if (eq == std::string::npos) {
                out[UrlDecode(pair)] = "";
            } else {
                out[UrlDecode(pair.substr(0, eq))] = UrlDecode(pair.substr(eq + 1));
            }
        }
        if (amp == std::string::npos) break;
        start = amp + 1;
    }
}

std::string MultipartBoundary(const std::string& contentType) {
    std::string lower = LowerCopy(contentType);
    auto pos = lower.find("boundary=");
    if (pos == std::string::npos) return "";
    std::string boundary = contentType.substr(pos + 9);
    auto semi = boundary.find(';');
    if (semi != std::string::npos) boundary = boundary.substr(0, semi);
    boundary = TrimCopy(boundary);
    if (!boundary.empty() && boundary.front() == '"') {
        boundary.erase(boundary.begin());
        if (!boundary.empty() && boundary.back() == '"') boundary.pop_back();
    }
    return boundary;
}

bool CaseInsensitiveLess(const std::string& a, const std::string& b) {
    const size_t shared = std::min(a.size(), b.size());
    for (size_t i = 0; i < shared; ++i) {
        const int ca = std::tolower(static_cast<unsigned char>(a[i]));
        const int cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb) return ca < cb;
    }
    return a.size() < b.size();
}

bool CaseInsensitiveEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

namespace PaymentSignature {

std::map<std::string, std::string> ParseForm(const std::string& contentType,
                                             const std::string& body) {
    std::map<std::string, std::string> out;
    std::string lower = LowerCopy(contentType);
    if (lower.find("multipart/form-data") != std::string::npos) {
        const std::string boundary = MultipartBoundary(contentType);
        if (boundary.empty()) return out;
        const std::string delimiter = "--" + boundary;
        size_t pos = 0;
        while ((pos = body.find(delimiter, pos)) != std::string::npos) {
            pos += delimiter.size();
            if (body.compare(pos, 2, "--") == 0) break;
            if (body.compare(pos, 2, "\r\n") == 0) pos += 2;
            size_t headerEnd = body.find("\r\n\r\n", pos);
            if (headerEnd == std::string::npos) break;
            std::string headers = body.substr(pos, headerEnd - pos);
            size_t valueStart = headerEnd + 4;
            size_t next = body.find("\r\n" + delimiter, valueStart);
            std::string value = body.substr(valueStart, next == std::string::npos
                                                           ? std::string::npos
                                                           : next - valueStart);
            std::string name;
            std::string lowerHeaders = LowerCopy(headers);
            auto cd = lowerHeaders.find("content-disposition:");
            if (cd != std::string::npos) {
                auto namePos = lowerHeaders.find("name=", cd);
                if (namePos != std::string::npos) {
                    namePos += 5;
                    if (namePos < headers.size() && headers[namePos] == '"') {
                        ++namePos;
                        auto endQuote = headers.find('"', namePos);
                        name = headers.substr(namePos, endQuote == std::string::npos
                                                          ? std::string::npos
                                                          : endQuote - namePos);
                    } else {
                        auto endSemi = headers.find_first_of(";\r\n", namePos);
                        name = headers.substr(namePos, endSemi == std::string::npos
                                                          ? std::string::npos
                                                          : endSemi - namePos);
                    }
                }
            }
            if (!name.empty()) out[name] = value;
            if (next == std::string::npos) break;
            pos = next + 2;
        }
        return out;
    }
    ParseUrlEncodedInto(body, out);
    return out;
}

std::string HmacSha256Hex(const std::string& key, const std::string& data) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest, &len);
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (unsigned int i = 0; i < len; ++i) {
        out.push_back(hex[digest[i] >> 4]);
        out.push_back(hex[digest[i] & 0x0F]);
    }
    return out;
}

std::string BillplzSource(const std::map<std::string, std::string>& params) {
    std::vector<std::string> pairs;
    pairs.reserve(params.size());
    for (const auto& [key, value] : params) {
        if (CaseInsensitiveEquals(key, "x_signature")) continue;
        pairs.push_back(key + value);
    }
    std::sort(pairs.begin(), pairs.end(), CaseInsensitiveLess);
    std::string source;
    for (const auto& pair : pairs) {
        if (!source.empty()) source += '|';
        source += pair;
    }
    return source;
}

std::string BillplzXSignature(const std::string& signatureKey,
                              const std::map<std::string, std::string>& params) {
    return HmacSha256Hex(signatureKey, BillplzSource(params));
}

} // namespace PaymentSignature
