#pragma once

// Printer-agnostic label drawing model.
//
// The layout code (GenericLabelPrinter) measures the label and produces
// LabelDoc primitives; a renderer translates those primitives into one
// printer command language. Nothing in this file depends on wxWidgets, so the
// document can also be used by the preview or by tests.

#include <string>
#include <utility>
#include <vector>

namespace LabelDoc {

struct Rect {
    int x {0};
    int y {0};
    int w {0};
    int h {0};
};

/// Page geometry, already converted to printer dots.
struct Profile {
    int widthDots {0};
    int heightDots {0};
    int gapXDots {0};
    int gapYDots {0};
    int dotsPerMm {8};
    int dpi {203};
    int copies {1};
    bool fromTop {true};
};

struct Text {
    Rect rect;
    std::string text;
    std::string fontCode;  // printer font id ("2", "A", ...)
    int rotation {0};
    int scaleX {1};
    int scaleY {1};
    bool reverse {false};  // draw the text inverted (white on black)
};

struct Barcode {
    Rect rect;
    std::string data;
    std::string symbology {"128"};
    int rotation {0};
    int narrow {2};
    int wide {2};
    bool withText {true};
};

struct QrCode {
    Rect rect;
    std::string data;
    int cell {4};  // module size in dots
    int rotation {0};
};

struct Box {
    Rect rect;
    int thickness {1};
};

/// Sequence of primitives in drawing order plus the page geometry.
struct Document {
    Profile profile;
    std::vector<Text> texts;
    std::vector<Barcode> barcodes;
    std::vector<QrCode> qrCodes;
    std::vector<Box> boxes;

    void Clear() {
        texts.clear();
        barcodes.clear();
        qrCodes.clear();
        boxes.clear();
    }

    bool Empty() const {
        return texts.empty() && barcodes.empty() && qrCodes.empty() && boxes.empty();
    }
};

/// Implemented once per command language (TSPL, ZPL, PPLA/PPLB, EPL, PPLZ,
/// CPCL, ESC/POS, EZPL). The renderer receives the byte sink from the caller,
/// so driver pass-through, RAW spool and TCP all share the same emitter.
class ILabelRenderer {
public:
    virtual ~ILabelRenderer() = default;

    /// Emit the job header and the page geometry.
    virtual void Begin(const Profile& profile) = 0;

    virtual void Draw(const Text& text) = 0;
    virtual void Draw(const Barcode& barcode) = 0;
    virtual void Draw(const QrCode& qr) = 0;
    virtual void Draw(const Box& box) = 0;

    /// Emit the job trailer (print/form feed and the copy count).
    virtual void End() = 0;
};

}  // namespace LabelDoc
