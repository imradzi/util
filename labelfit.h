#pragma once

#include <string>
#include <vector>

/// Fit-to-label arithmetic: decides how many text lines a label can hold and
/// which optional blocks have to be dropped before the label is rendered.
/// Pure standard C++; the printout supplies the real font heights in dots.
namespace LabelFit {

struct Fonts {
    int bannerHeight {12};
    int bodyHeight {20};
    int tinyHeight {12};
    int priceHeight {32};
    int lineGap {0};
};

struct Options {
    int dotsPerMm {8};
    bool useQR {false};
    bool printBarcodeFirst {true};
    bool hasPack {false};
    bool hasExpiry {false};
    bool hasPatient {false};
    bool dispensing {false};
    bool hasWarning {false};
    bool isPoison {false};
};

struct Result {
    bool refused {false};
    std::string refusalReason;

    bool showBanner {true};
    bool showPack {true};
    bool showId {true};
    bool showDate {true};
    bool showExpiry {true};

    int gap {2};
    int bannerHeight {0};
    int nameLines {1};
    int nameBlockHeight {0};
    int packHeight {0};
    int priceHeight {0};
    int idHeight {0};
    int barcodeHeight {0};
    int qrCell {0};
    int qrHeight {0};
    int qtyWidth {0};
    int instructionLines {2};
    int warningHeight {0};
    int poisonHeight {0};
    int contentHeight {0};

    /// Human-readable list of optional blocks that no longer fit.
    std::vector<std::string> dropped;
};

/// Price-tag layout: banner, item name, pack description, id text, price and
/// either a 1-D barcode or a QR code.
Result Compute(int widthDots, int heightDots, const Fonts& fonts, const Options& options);

/// Dispensing label: patient, item, instruction, quantity/barcode and the
/// mandatory warning / controlled-drug lines. The warning and poison line are
/// never dropped: if they cannot fit, `refused` is set.
Result ComputeDispensing(int widthDots, int heightDots, const Fonts& fonts, const Options& options);

}  // namespace LabelFit
