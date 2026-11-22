#pragma once

#include <string>

/// Raw print-queue helpers shared by label printing, cash-drawer kicks and
/// setup test pages. Thin wrapper over CashDrawerKick so the Windows RAW
/// spooling rules live in one place.
namespace RawPrint {

/// True when `device` names a Windows print queue rather than a DOS/device
/// port. A UNC share or a plain printer name is a queue.
bool IsSpoolDevice(const std::string& device);

/// Sends `bytes` to the printer queue as one RAW job. Returns false on
/// non-Windows builds, when the queue rejects RAW data, or when the printer
/// reports a blocking status.
bool SpoolRaw(const std::string& queue, const std::string& bytes, std::string& error);

}  // namespace RawPrint
