#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

/// Shared fee math for outlet-defined online payment fees.
/// fee = clamp(round(order * percent/100 + fixed), min, max); 0 = no bound.
namespace PaymentFee {
    inline int64_t ComputeCents(int64_t orderAmountCents, double percent,
                                int64_t fixedCents, int64_t minCents, int64_t maxCents) {
        if (orderAmountCents <= 0) return 0;
        double raw = static_cast<double>(orderAmountCents) * percent / 100.0 +
                     static_cast<double>(fixedCents);
        int64_t fee = static_cast<int64_t>(std::llround(raw));
        if (fee < 0) fee = 0;
        if (minCents > 0 && fee < minCents) fee = minCents;
        if (maxCents > 0 && fee > maxCents) fee = maxCents;
        return fee;
    }
}
