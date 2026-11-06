#pragma once

#include <cstdint>
#include <string>
#include <vector>

/// Payment-statement parsing rules shared by the importer and its tests.
/// All amounts are returned in cents so the ledger never depends on binary
/// floating-point rounding.
namespace PaymentCsv {

struct Mapping {
    int date = -1;
    int description = -1;
    int reference = -1;
    int gross = -1;
    int fee = -1;
    int net = -1;
    int billRef = -1;
    int batchRef = -1;

    bool HasAmount() const { return gross >= 0 || net >= 0; }
    bool Valid() const { return date >= 0 && HasAmount(); }
};

/// Lowercase and keep only [a-z0-9]. "Bill Code" and "bill_code" both become
/// "billcode".
std::string NormalizeHeader(const std::string& value);

/// True when the row looks like column labels rather than data (at least two
/// cells match a known header alias).
bool LooksLikeHeader(const std::vector<std::string>& row);

/// Guess the column mapping. `importType` is "bank", "edc" or "provider" and
/// only changes how a generic "amount"/"credit" column is interpreted.
Mapping SuggestMapping(const std::vector<std::string>& headers,
                       const std::string& importType);

/// Parse common Malaysian bank / provider date formats. Times are optional;
/// values are interpreted in the outlet's local timezone and returned as ms.
bool ParseDateMs(const std::string& value, int64_t& out);

/// Parse "1,234.56", "RM 1,234.56", "(12.34)", "12.34-", "1.234,56".
bool ParseAmountCents(const std::string& value, int64_t& out);

} // namespace PaymentCsv
