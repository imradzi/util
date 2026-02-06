#include <string>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>

// Base64URL encoding (URL-safe, no padding)
static std::string base64url_encode(const unsigned char* data, size_t len) {
    constexpr char base64url_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                          "abcdefghijklmnopqrstuvwxyz"
                                          "0123456789-_";

    std::string encoded;
    encoded.reserve(((len * 4) + 2) / 3);  // Optimal capacity

    size_t i = 0;
    while (i + 2 < len) {  // Process 3-byte groups
        uint32_t triple = (static_cast<uint32_t>(data[i]) << 16) | (static_cast<uint32_t>(data[i + 1]) << 8) | static_cast<uint32_t>(data[i + 2]);
        i += 3;

        encoded.push_back(base64url_alphabet[(triple >> 18) & 0x3F]);
        encoded.push_back(base64url_alphabet[(triple >> 12) & 0x3F]);
        encoded.push_back(base64url_alphabet[(triple >> 6) & 0x3F]);
        encoded.push_back(base64url_alphabet[triple & 0x3F]);
    }

    // Handle remaining bytes (1 or 2)
    if (i < len) {
        uint32_t last = data[i++];
        encoded.push_back(base64url_alphabet[last >> 2]);
        if (i == len) {
            encoded.push_back(base64url_alphabet[(last & 0x03) << 4]);
        } else {
            last = (last << 8) | data[i];
            encoded.push_back(base64url_alphabet[(last >> 4) & 0x3F]);
            encoded.push_back(base64url_alphabet[(last & 0x0F) << 2]);
        }
    }

    return encoded;
}

// Generate 7-character UUID (uses 7 bytes)
std::string generateShortUUID() {
    extern boost::uuids::random_generator_mt19937 uuidGen;
    boost::uuids::uuid uuid = uuidGen();

    // Use first 7 bytes of UUID (56 bits)
    return base64url_encode(uuid.data(), 7);
}