#include "paymentcsv.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <initializer_list>
#include <string>

namespace PaymentCsv {

namespace {

std::string TrimCopy(const std::string& value) {
    size_t begin = 0;
    size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(begin, end - begin);
}

std::string LowerCopy(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool ContainsAny(const std::string& normalized,
                 std::initializer_list<const char*> aliases) {
    for (const char* alias : aliases) {
        if (normalized == alias) return true;
    }
    for (const char* alias : aliases) {
        if (normalized.find(alias) != std::string::npos) return true;
    }
    return false;
}

int FindHeader(const std::vector<std::string>& headers,
               std::initializer_list<const char*> aliases,
               const std::vector<bool>& used) {
    // Prefer an exact normalized match, then a contains match.
    for (size_t i = 0; i < headers.size(); ++i) {
        if (used[i]) continue;
        if (NormalizeHeader(headers[i]) == *aliases.begin()) return static_cast<int>(i);
    }
    for (size_t i = 0; i < headers.size(); ++i) {
        if (used[i]) continue;
        for (const char* alias : aliases) {
            if (NormalizeHeader(headers[i]) == alias) return static_cast<int>(i);
        }
    }
    for (size_t i = 0; i < headers.size(); ++i) {
        if (used[i]) continue;
        const std::string normalized = NormalizeHeader(headers[i]);
        for (const char* alias : aliases) {
            if (normalized.find(alias) != std::string::npos) return static_cast<int>(i);
        }
    }
    return -1;
}

void MarkUsed(std::vector<bool>& used, int index) {
    if (index >= 0 && index < static_cast<int>(used.size())) used[index] = true;
}

int MonthFromName(const std::string& name) {
    static const char* months[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                   "jul", "aug", "sep", "oct", "nov", "dec"};
    const std::string lower = LowerCopy(name.substr(0, 3));
    for (int i = 0; i < 12; ++i)
        if (lower == months[i]) return i + 1;
    return 0;
}

bool ParseInt(const std::string& value, int& out) {
    if (value.empty()) return false;
    for (char c : value)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    out = std::atoi(value.c_str());
    return true;
}

} // namespace

std::string NormalizeHeader(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

bool LooksLikeHeader(const std::vector<std::string>& row) {
    int matched = 0;
    for (const auto& cell : row) {
        const std::string name = NormalizeHeader(cell);
        if (name.empty()) continue;
        if (ContainsAny(name, {"date", "tarikh", "amount", "gross", "net", "credit",
                               "debit", "fee", "charge", "mdr", "description",
                               "details", "particulars", "reference", "refno",
                               "billcode", "billid", "batch", "rrn"}))
            ++matched;
    }
    return matched >= 2;
}

Mapping SuggestMapping(const std::vector<std::string>& headers,
                       const std::string& importType) {
    Mapping mapping;
    std::vector<bool> used(headers.size(), false);
    const bool bank = (importType == "bank");

    mapping.date = FindHeader(headers, {"date", "transactiondate", "txndate",
                                        "tarikh", "postingdate", "valuedate",
                                        "billdate", "paymentdate"}, used);
    MarkUsed(used, mapping.date);

    mapping.description = FindHeader(headers, {"description", "details",
                                               "particulars", "narrative",
                                               "keterangan", "remark", "memo",
                                               "transactiondescription"}, used);
    MarkUsed(used, mapping.description);

    mapping.reference = FindHeader(headers, {"reference", "ref", "refno",
                                             "referenceno", "transactionid",
                                             "txnid", "chequeno", "cheque",
                                             "documentno"}, used);
    MarkUsed(used, mapping.reference);

    mapping.billRef = FindHeader(headers, {"billcode", "billid", "billno",
                                           "billreference", "invoiceno",
                                           "invoice", "invoicenumber",
                                           "orderid", "orderno"}, used);
    MarkUsed(used, mapping.billRef);

    mapping.batchRef = FindHeader(headers, {"batch", "batchno", "batchid",
                                            "settlementbatch", "settlementid",
                                            "rrn", "approvalcode"}, used);
    MarkUsed(used, mapping.batchRef);

    mapping.fee = FindHeader(headers, {"fee", "fees", "charge", "charges", "mdr",
                                       "servicecharge", "transactioncharge",
                                       "cost"}, used);
    MarkUsed(used, mapping.fee);

    if (bank) {
        mapping.net = FindHeader(headers, {"credit", "creditamount", "amountcredited",
                                           "credited", "net", "netamount", "payout",
                                           "settlementamount", "amount"}, used);
        MarkUsed(used, mapping.net);
        mapping.gross = FindHeader(headers, {"gross", "grossamount", "billamount",
                                             "total"}, used);
        MarkUsed(used, mapping.gross);
    } else {
        mapping.gross = FindHeader(headers, {"gross", "grossamount", "amount",
                                             "billamount", "paidamount", "total",
                                             "collection"}, used);
        MarkUsed(used, mapping.gross);
        mapping.net = FindHeader(headers, {"net", "netamount", "payout",
                                           "settlementamount", "credited",
                                           "amountcredited"}, used);
        MarkUsed(used, mapping.net);
    }
    return mapping;
}

bool ParseDateMs(const std::string& value, int64_t& out) {
    std::string text = TrimCopy(value);
    if (text.empty()) return false;

    // Strip an optional timestamp after 'T' or a space. Only split on a space
    // when the tail actually looks like a time, otherwise "06 Nov 2026" would
    // be cut after the day.
    std::string datePart = text;
    std::string timePart;
    const size_t tPos = text.find('T');
    size_t split = std::string::npos;
    if (tPos != std::string::npos) {
        split = tPos;
    } else {
        const size_t lastSpace = text.rfind(' ');
        if (lastSpace != std::string::npos &&
            text.find(':', lastSpace + 1) != std::string::npos) {
            split = lastSpace;
        }
    }
    if (split != std::string::npos) {
        datePart = text.substr(0, split);
        timePart = TrimCopy(text.substr(split + 1));
    }

    int year = 0, month = 0, day = 0;
    std::vector<std::string> parts;
    std::string token;
    for (char c : datePart) {
        if (c == '-' || c == '/' || c == '.' ||
            std::isspace(static_cast<unsigned char>(c))) {
            if (!token.empty()) {
                parts.push_back(token);
                token.clear();
            }
        } else {
            token.push_back(c);
        }
    }
    if (!token.empty()) parts.push_back(token);

    if (parts.size() == 3) {
        int a = 0, b = 0, c = 0;
        if (!ParseInt(parts[0], a) || !ParseInt(parts[1], b) || !ParseInt(parts[2], c)) {
            // DD MMM YYYY
            const int monthName = MonthFromName(parts[1]);
            if (monthName > 0 && ParseInt(parts[0], a) && ParseInt(parts[2], c)) {
                day = a;
                month = monthName;
                year = c;
            } else {
                return false;
            }
        } else if (parts[0].size() == 4) {
            year = a;
            month = b;
            day = c;
        } else {
            day = a;
            month = b;
            year = c;
        }
    } else {
        return false;
    }

    if (year < 100) year += (year <= 69 ? 2000 : 1900);
    if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31) return false;

    int hour = 0, minute = 0, second = 0;
    if (!timePart.empty()) {
        std::vector<int> timeParts;
        std::string num;
        for (char ch : timePart) {
            if (ch == ':') {
                if (!num.empty()) { timeParts.push_back(std::atoi(num.c_str())); num.clear(); }
            } else if (std::isdigit(static_cast<unsigned char>(ch))) {
                num.push_back(ch);
            } else {
                break;
            }
        }
        if (!num.empty()) timeParts.push_back(std::atoi(num.c_str()));
        if (timeParts.size() >= 2) {
            hour = timeParts[0];
            minute = timeParts[1];
            if (timeParts.size() >= 3) second = timeParts[2];
        }
    }

    std::tm t{};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;
    t.tm_isdst = -1;
    const std::time_t epoch = std::mktime(&t);
    if (epoch == static_cast<std::time_t>(-1)) return false;
    out = static_cast<int64_t>(epoch) * 1000LL;
    return true;
}

bool ParseAmountCents(const std::string& value, int64_t& out) {
    std::string text = TrimCopy(value);
    if (text.empty()) return false;

    bool negative = false;
    if (text.size() >= 2 && text.front() == '(' && text.back() == ')') {
        negative = true;
        text = text.substr(1, text.size() - 2);
    }
    std::string lower = LowerCopy(TrimCopy(text));
    if (lower.size() >= 2 && lower.compare(lower.size() - 2, 2, "dr") == 0) {
        negative = true;
        text = text.substr(0, text.size() - 2);
    } else if (lower.size() >= 2 && lower.compare(lower.size() - 2, 2, "cr") == 0) {
        text = text.substr(0, text.size() - 2);
    }
    text = TrimCopy(text);
    if (!text.empty() && text.back() == '-') {
        negative = true;
        text.pop_back();
    }
    if (!text.empty() && text.front() == '-') {
        negative = true;
        text.erase(text.begin());
    }

    std::string number;
    for (char c : text) {
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == ',')
            number.push_back(c);
    }
    if (number.empty()) return false;

    const size_t lastDot = number.rfind('.');
    const size_t lastComma = number.rfind(',');
    if (lastDot != std::string::npos && lastComma != std::string::npos) {
        if (lastDot > lastComma) {
            number.erase(std::remove(number.begin(), number.end(), ','), number.end());
        } else {
            number.erase(std::remove(number.begin(), number.end(), '.'), number.end());
            std::replace(number.begin(), number.end(), ',', '.');
        }
    } else if (lastComma != std::string::npos) {
        const size_t commaCount = static_cast<size_t>(std::count(number.begin(), number.end(), ','));
        const size_t digitsAfter = number.size() - lastComma - 1;
        if (commaCount == 1 && digitsAfter == 2) {
            std::replace(number.begin(), number.end(), ',', '.');
        } else {
            number.erase(std::remove(number.begin(), number.end(), ','), number.end());
        }
    }

    try {
        const double amount = std::stod(number);
        const int64_t cents = static_cast<int64_t>(std::llround(amount * 100.0));
        out = negative ? -cents : cents;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace PaymentCsv
