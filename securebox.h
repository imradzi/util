#pragma once

#include <string>

/// Small AES-256-GCM helper for payment gateway credentials at rest.
/// Payload format: "enc:v1:<nonce-base64>:<ciphertext+tag-base64>".
/// Plain values without the prefix are accepted by Decrypt so an operator can
/// paste a value manually and it is re-encrypted on the next save.
namespace SecureBox {
    /// Generate a fresh base64-encoded 32-byte key (for first installation).
    std::string GenerateKey();

    /// Encrypt plaintext with a base64-encoded 32-byte key.
    std::string Encrypt(const std::string& keyBase64, const std::string& plaintext);

    /// Decrypt a payload. Returns the input unchanged when it has no prefix.
    std::string Decrypt(const std::string& keyBase64, const std::string& payload);

    bool IsEncrypted(const std::string& payload);
}
