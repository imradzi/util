#pragma once

#include <map>
#include <string>

/// Signature helpers shared by the payment provider adapters.
///
/// Deliberately free of DB / proto / network dependencies so the algorithms can
/// be unit-tested on their own (see tests/test_billplz_signature.cpp).
namespace PaymentSignature {

/// Decode an application/x-www-form-urlencoded (or multipart/form-data) body
/// into name/value pairs. Values are URL-decoded; a duplicate name wins last.
std::map<std::string, std::string> ParseForm(const std::string& contentType,
                                             const std::string& body);

/// Lowercase-hex HMAC-SHA256(key, data).
std::string HmacSha256Hex(const std::string& key, const std::string& data);

/// Billplz X-Signature source string.
///
/// Every posted pair except `x_signature`, rendered as `key + value`, sorted
/// case-insensitively ASCENDING and joined with '|'.
///
/// Billplz sorts the CONCATENATED pairs, not the bare keys. Because `paid` is a
/// prefix of `paid_amount` and `paid_at`, a key-only sort puts `paidtrue` before
/// both of them and the HMAC never matches a real callback.
std::string BillplzSource(const std::map<std::string, std::string>& params);

/// Billplz X-Signature: lowercase-hex HMAC-SHA256(signatureKey, BillplzSource).
std::string BillplzXSignature(const std::string& signatureKey,
                              const std::map<std::string, std::string>& params);

} // namespace PaymentSignature
