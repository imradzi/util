#pragma once

#include <string>

/// Cash-drawer kick-out helpers shared by ppClient and ppSetup.
///
/// The drawer is a 24 V device on the receipt printer's kick-out port, so the
/// only thing that ever opens it is the ESC/POS command the *printer* receives
/// as raw bytes. Two backends can carry those bytes:
///
///   spool  the printer's own Windows queue (USB, network, shared). The client
///          writes one RAW print job; no driver renders anything, so every
///          ESC/POS printer behaves the same.
///   port   a DOS port a printer share was mapped to ("net use LPT2: ...") or
///          a real serial device, written to as a plain file. Kept for config
///          files written by older setup versions.
///
/// Everything here is standard C++ (plus Win32 behind #ifdef) so the rules can
/// be unit-tested without wxWidgets, boost, a printer or a spooler.
namespace CashDrawerKick {

/// True when `device` names a print queue and therefore uses the spool
/// backend. "LPT2:", "COM3:", "/dev/usb/lp0" and "\\.\LPT1" are ports; a UNC
/// share ("\\server\printer") or a plain name ("XP-80C") is a queue. An empty
/// device names no backend at all.
bool IsSpoolDevice(const std::string& device);

/// The bytes that kick the drawer. "esc/pos" (case-insensitive, also an empty
/// value) produces ESC p <pin> <on> <off> - the same five bytes the LPT path
/// has always written, with `pin` 0 = drawer 1 (pin 2) and 1 = drawer 2
/// (pin 5). `pulseMs` is clamped to 20..500 and expressed in the 2 ms unit the
/// command uses. Any other `command` is drawer-specific text and is returned
/// verbatim.
std::string BuildKickBytes(const std::string& command, int pin, int pulseMs);

/// True when the port backend can still reach `device`. Called once while the
/// configuration is read, so a legacy "net use LPT2:" mapping that has gone
/// away can fall back to the receipt printer's queue instead of silently doing
/// nothing. Always false for a spool device or an empty name.
bool CanOpenPortDevice(const std::string& device);

/// Sends `bytes` to the print queue `queue` as one RAW job (no rendering, no
/// page, no paper feed). Refuses when the printer reports a blocking status,
/// because a job that only leaves the spooler when the printer is switched on
/// again would open the drawer minutes later, in front of the wrong cashier.
/// `error` carries the reason on failure. Returns false on non-Windows builds
/// and when the queue rejects raw data (v4/class drivers), so callers can keep
/// the port path as a fallback.
bool SpoolRaw(const std::string& queue, const std::string& bytes, std::string& error);

} // namespace CashDrawerKick
