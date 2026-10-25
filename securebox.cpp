#include "securebox.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include "encrypt64.h"

namespace {

constexpr const char* kPrefix = "enc:v1:";
constexpr int kKeyBytes = 32;
constexpr int kNonceBytes = 12;
constexpr int kTagBytes = 16;

std::string Encode(const std::string& raw) {
    return UsingBoost::encode(raw);
}

std::string Decode(const std::string& b64) {
    return UsingBoost::decode(b64);
}

std::vector<unsigned char> KeyFromBase64(const std::string& keyBase64) {
    auto key = Decode(keyBase64);
    if (key.size() != kKeyBytes)
        throw std::runtime_error("SecureBox: key must be 32 bytes");
    return std::vector<unsigned char>(key.begin(), key.end());
}

} // namespace

namespace SecureBox {

bool IsEncrypted(const std::string& payload) {
    return payload.rfind(kPrefix, 0) == 0;
}

std::string GenerateKey() {
    unsigned char key[kKeyBytes];
    if (RAND_bytes(key, kKeyBytes) != 1)
        throw std::runtime_error("SecureBox: RAND_bytes failed");
    return Encode(std::string(reinterpret_cast<char*>(key), kKeyBytes));
}

std::string Encrypt(const std::string& keyBase64, const std::string& plaintext) {
    if (plaintext.empty()) return "";
    auto key = KeyFromBase64(keyBase64);

    unsigned char nonce[kNonceBytes];
    if (RAND_bytes(nonce, kNonceBytes) != 1)
        throw std::runtime_error("SecureBox: RAND_bytes failed");

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("SecureBox: EVP_CIPHER_CTX_new failed");

    std::vector<unsigned char> cipher(plaintext.size() + 16);
    int len = 0;
    int outLen = 0;
    try {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
            throw std::runtime_error("SecureBox: EncryptInit failed");
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceBytes, nullptr) != 1)
            throw std::runtime_error("SecureBox: set IV length failed");
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce) != 1)
            throw std::runtime_error("SecureBox: key/IV init failed");
        if (EVP_EncryptUpdate(ctx, cipher.data(), &len,
                              reinterpret_cast<const unsigned char*>(plaintext.data()),
                              static_cast<int>(plaintext.size())) != 1)
            throw std::runtime_error("SecureBox: EncryptUpdate failed");
        outLen = len;
        if (EVP_EncryptFinal_ex(ctx, cipher.data() + outLen, &len) != 1)
            throw std::runtime_error("SecureBox: EncryptFinal failed");
        outLen += len;

        unsigned char tag[kTagBytes];
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagBytes, tag) != 1)
            throw std::runtime_error("SecureBox: get tag failed");
        cipher.resize(outLen);
        cipher.insert(cipher.end(), tag, tag + kTagBytes);
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }
    EVP_CIPHER_CTX_free(ctx);

    std::string nonceStr(reinterpret_cast<char*>(nonce), kNonceBytes);
    std::string cipherStr(reinterpret_cast<char*>(cipher.data()), cipher.size());
    return std::string(kPrefix) + Encode(nonceStr) + ":" + Encode(cipherStr);
}

std::string Decrypt(const std::string& keyBase64, const std::string& payload) {
    if (payload.empty()) return "";
    if (!IsEncrypted(payload)) return payload;  // plaintext compatibility

    const std::string rest = payload.substr(std::string(kPrefix).size());
    auto sep = rest.find(':');
    if (sep == std::string::npos)
        throw std::runtime_error("SecureBox: malformed payload");

    auto nonce = Decode(rest.substr(0, sep));
    auto cipher = Decode(rest.substr(sep + 1));
    if (nonce.size() != kNonceBytes || cipher.size() < kTagBytes)
        throw std::runtime_error("SecureBox: invalid nonce/tag");

    auto key = KeyFromBase64(keyBase64);
    const int dataLen = static_cast<int>(cipher.size()) - kTagBytes;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("SecureBox: EVP_CIPHER_CTX_new failed");

    std::vector<unsigned char> plain(cipher.size());
    int len = 0;
    int outLen = 0;
    try {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
            throw std::runtime_error("SecureBox: DecryptInit failed");
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceBytes, nullptr) != 1)
            throw std::runtime_error("SecureBox: set IV length failed");
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(),
                               reinterpret_cast<const unsigned char*>(nonce.data())) != 1)
            throw std::runtime_error("SecureBox: key/IV init failed");
        if (EVP_DecryptUpdate(ctx, plain.data(), &len,
                              reinterpret_cast<const unsigned char*>(cipher.data()), dataLen) != 1)
            throw std::runtime_error("SecureBox: DecryptUpdate failed");
        outLen = len;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagBytes,
                                reinterpret_cast<unsigned char*>(cipher.data()) + dataLen) != 1)
            throw std::runtime_error("SecureBox: set tag failed");
        if (EVP_DecryptFinal_ex(ctx, plain.data() + outLen, &len) != 1)
            throw std::runtime_error("SecureBox: authentication failed");
        outLen += len;
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }
    EVP_CIPHER_CTX_free(ctx);

    return std::string(reinterpret_cast<char*>(plain.data()), outLen);
}

} // namespace SecureBox
