#pragma once
#include <string>

namespace UsingBeast {
    std::vector<uint8_t> base64_decode(const std::string& input);
    std::string clean_base64(const std::string& input);
    std::string encode(const std::string& s);
    inline std::string decode(const std::string& s) {return clean_base64(s); }
}

namespace UsingBoost {
    std::string encode(const std::string& s);
    std::string decode(const std::string& s);
    std::string sha256(const std::string& s);
}

namespace UsingOpenSSL {
    std::string encode(const std::string& s);
    std::string decode(const std::string& s);
    std::string sha256(const std::string& s);
}