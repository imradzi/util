#include "rawprint.h"

#include "cashdrawerkick.h"

namespace RawPrint {

bool IsSpoolDevice(const std::string& device) {
    return CashDrawerKick::IsSpoolDevice(device);
}

bool SpoolRaw(const std::string& queue, const std::string& bytes, std::string& error) {
    return CashDrawerKick::SpoolRaw(queue, bytes, error);
}

}  // namespace RawPrint
