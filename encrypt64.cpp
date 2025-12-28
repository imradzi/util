#include "encrypt64.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>  // For trim_right_copy_if
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/beast/core/detail/base64.hpp>
// #include <boost/hash2/sha256.hpp>
// #include <boost/hash2/hash_append.hpp>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

static char* base64_encode(const unsigned char* input, int length) {
    // Create a memory BIO to hold raw data
    BIO* bmem = BIO_new(BIO_s_mem());
    // Create a Base64 filter BIO and chain it to the memory BIO
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_push(b64, bmem);

    // Disable line breaks (optional)
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    // Write data to the Base64 BIO (encodes automatically)
    BIO_write(b64, input, length);
    BIO_flush(b64);  // Ensure all data is encoded

    // Get pointer to encoded data in the memory BIO
    char* encoded = NULL;
    long len = BIO_get_mem_data(bmem, &encoded);

    // Copy result to a new string (optional, if you need to use it after BIO cleanup)
    char* result = (char*)malloc(len + 1);
    memcpy(result, encoded, len);
    result[len] = '\0';  // Null-terminate

    // Free BIO chain resources
    BIO_free_all(b64);
    return result;
}

static void handle_openssl_error() {
    throw ("ssl fatal error");
}

static void compute_sha256_evp(const char* str, unsigned char hash[EVP_MAX_MD_SIZE]) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) handle_openssl_error();

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1)
        handle_openssl_error();
    if (EVP_DigestUpdate(ctx, str, strlen(str)) != 1)
        handle_openssl_error();

    unsigned int len;
    if (EVP_DigestFinal_ex(ctx, hash, &len) != 1)
        handle_openssl_error();

    EVP_MD_CTX_free(ctx);
}

std::string UsingOpenSSL::sha256(const std::string &src) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    compute_sha256_evp(src.c_str(), hash);
    std::string res;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        res.append(fmt::format("{:02x}", hash[i]));
    }
    return res;
}

std::string UsingOpenSSL::encode(const std::string &src) {
    char* encoded = base64_encode((const unsigned char*)src.c_str(), src.length());
    std::string res(encoded);
    free(encoded);  // Free the result buffer
    return res;
}

std::string UsingOpenSSL::decode(const std::string &src) {
    return "";
}


// std::string UsingBeast::encode(const std::string& input) {
//     return boost::beast::detail::base64::encode((void *)input.data(), input.length());
//     return "";
// }

// std::vector<uint8_t> UsingBeast::base64_decode(const std::string& input) {
//     auto result = boost::beast::detail::base64::decode((void *)input.data(), input.length());
//     result.first.resize(result.second);  // Resize to actual decoded bytes
//     return result.first;
// }

// std::string UsingBeast::clean_base64(const std::string& input) {
//     std::string clean;
//     for (char c : input) {
//         if (isalnum(c) || c == '+' || c == '/' || c == '=')
//             clean.push_back(c);
//     }
//     return clean;

// }

// Base64 Encoding
std::string UsingBoost::encode(const std::string& input) {
    using namespace boost::archive::iterators;
    using Base64Encoder = base64_from_binary<transform_width<std::string::const_iterator, 6, 8>>;
    std::string encoded(Base64Encoder(input.begin()), Base64Encoder(input.end()));

    // Add padding (if needed)
    size_t padding = (3 - input.size() % 3) % 3;
    for (size_t i = 0; i < padding; i++) {
        encoded.push_back('=');
    }

    return encoded;
}

// Base64 Decoding
std::string UsingBoost::decode(const std::string& input) {
    using namespace boost::archive::iterators;

    // Create a sanitized copy without non-Base64 characters
    std::string sanitized;
    std::copy_if(input.begin(), input.end(), std::back_inserter(sanitized),
        [](char c) {
            return isalnum(c) || c == '+' || c == '/' || c == '=';
        });

    // Calculate padding to remove
    size_t padding = std::count(sanitized.begin(), sanitized.end(), '=');
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '='), sanitized.end());

    using Base64Decoder = transform_width<binary_from_base64<std::string::const_iterator>, 8, 6>;

    try {
        std::string decoded(Base64Decoder(sanitized.begin()), Base64Decoder(sanitized.end()));

        // Remove null bytes added during padding calculations
        decoded.erase(decoded.end() - padding, decoded.end());
        return decoded;
    } catch (const boost::archive::iterators::dataflow_exception&) {
        throw std::runtime_error("Invalid base64 input");
    }
}

// std::string UsingBoost::sha256(const std::string &input) {
//     // boost::hash2::sha256 sha;
//     // sha.update(input.data(), input.size());
//     // auto digest = sha.final();

//     // for (uint8_t byte : digest) {
//     //     std::cout << std::hex << std::setw(2) << std::setfill('0')
//     //               << static_cast<int>(byte);
//     // }
//     // return 0;
//     return "";
// }

// // Example usage
// int main() {
//     std::string original = "Boost Serialization Base64 Example!";

//     // Encode
//     std::string encoded = base64_encode(original);
//     std::cout << "Encoded: " << encoded << "\n";
//     // Output: Qm9vc3QgU2VyaWFsaXphdGlvbiBCYXNlNjQgRXhhbXBsZSE=

//     // Decode
//     std::string decoded = base64_decode(encoded);
//     std::cout << "Decoded: " << decoded << "\n";
//     // Output: Boost Serialization Base64 Example!

//     return 0;
// }