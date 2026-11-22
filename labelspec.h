#pragma once

#include <cmath>
#include <string>

/// Physical label geometry shared by the printout code, the settings dialog
/// and the setup tool. Pure standard C++ so the rules can be unit-tested
/// without wxWidgets, boost or a printer.
namespace LabelSpec {

/// Label size and feed configuration, all dimensions in millimetres except
/// dpi and rotation (degrees, clockwise).
struct Profile {
    double widthMm {35.0};
    double heightMm {25.0};
    double gapXMm {1.0};
    double gapYMm {3.0};
    int dpi {203};
    int rotation {0};  // 0, 90, 180 or 270

    /// Printer dots per millimetre. 203 dpi is the common 8 dots/mm head;
    /// 200 is accepted as an alias written by older setup versions.
    int DotsPerMm() const {
        if (dpi <= 0) return 0;
        const int dots = static_cast<int>(std::lround(static_cast<double>(dpi) / 25.4));
        return dots > 0 ? dots : 1;
    }

    int WidthDots() const {
        const int dpm = DotsPerMm();
        return dpm > 0 ? static_cast<int>(std::lround(widthMm * dpm)) : 0;
    }

    int HeightDots() const {
        const int dpm = DotsPerMm();
        return dpm > 0 ? static_cast<int>(std::lround(heightMm * dpm)) : 0;
    }
};

/// True when the profile is inside the ranges the client and setup accept.
/// `reason` receives a user-facing explanation on failure.
inline bool Validate(const Profile& p, std::string& reason) {
    if (p.widthMm < 10.0 || p.widthMm > 200.0) {
        reason = "label width must be between 10 and 200 mm";
        return false;
    }
    if (p.heightMm < 10.0 || p.heightMm > 300.0) {
        reason = "label height must be between 10 and 300 mm";
        return false;
    }
    if (p.dpi != 200 && p.dpi != 203 && p.dpi != 300 && p.dpi != 600) {
        reason = "print head resolution must be 203, 300 or 600 dpi";
        return false;
    }
    if (p.gapXMm < 0.0 || p.gapXMm > 10.0 || p.gapYMm < 0.0 || p.gapYMm > 10.0) {
        reason = "label gap must be between 0 and 10 mm";
        return false;
    }
    if (p.rotation != 0 && p.rotation != 90 && p.rotation != 180 && p.rotation != 270) {
        reason = "label rotation must be 0, 90, 180 or 270 degrees";
        return false;
    }
    return true;
}

inline int NormaliseRotation(int degrees) {
    int r = degrees % 360;
    if (r < 0) r += 360;
    return r;
}

}  // namespace LabelSpec
