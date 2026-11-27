#include "labelfit.h"

#include <algorithm>
#include <cmath>

namespace {

int ClampInt(int value, int low, int high) {
    return std::max(low, std::min(value, high));
}

int GapFor(int widthDots, int heightDots) {
    const int shortest = std::min(widthDots, heightDots);
    return ClampInt(shortest / 100, 1, 8);
}

int BarcodeBlockHeight(int heightDots, const LabelFit::Fonts& fonts) {
    return ClampInt(static_cast<int>(std::lround(heightDots * 0.20)), 20, 90) +
           fonts.tinyHeight + 2;
}

int QrBlockHeight(int& qrCell) {
    // Reserve enough for a version-2 QR (25 modules) plus the 4-module quiet
    // zone on each side. TSPL/ZPL choose the version from the data, so a
    // longer payload can still use a larger version; this is the common case
    // for a label id.
    qrCell = ClampInt(qrCell, 2, 6);
    return qrCell * 33;
}

}  // namespace

namespace LabelFit {

Result Compute(int widthDots, int heightDots, const Fonts& fonts, const Options& options) {
    Result result;
    if (widthDots <= 16 || heightDots <= 16) {
        result.refused = true;
        result.refusalReason = "the label is too small to print anything";
        return result;
    }

    result.gap = GapFor(widthDots, heightDots);
    result.bannerHeight = fonts.bannerHeight + 2;
    result.packHeight = fonts.bodyHeight;
    // The pack description shares the price line, so the block is as tall as
    // the taller of the two.
    result.priceHeight = std::max(fonts.priceHeight + 2, fonts.bodyHeight);
    result.idHeight = fonts.tinyHeight;
    const int lineHeight = fonts.bodyHeight + std::max(0, fonts.lineGap);

    result.showBanner = heightDots >= 15 * options.dotsPerMm;
    result.showPack = options.hasPack && heightDots >= 18 * options.dotsPerMm;
    result.showId = heightDots >= 20 * options.dotsPerMm;
    if (options.hasPack && !result.showPack) {
        result.dropped.emplace_back("pack description hidden - label is too short");
    }

    int barcodeHeight = BarcodeBlockHeight(heightDots, fonts);
    int qrCell = ClampInt(std::min(widthDots, heightDots) / 60, 2, 6);

    // The description is the only block that cannot be placed without knowing
    // how much room it needs, so the caller passes the wrapped line count. When
    // the caller does not know (the settings preview) two lines are assumed: a
    // single line is usually just the "[code] " prefix and the item name never
    // reaches the label.
    const int desiredNameLines = options.nameLinesNeeded > 0 ? options.nameLinesNeeded : 2;

    // Fixed blocks only: the price is pinned to the bottom edge (with the item
    // id line just above it) and the barcode/QR keeps the position the setting
    // asks for - at the top, or in the lower half directly above the price
    // when "barcode first" is off. The item name takes every line that is left
    // between them, so a long description never runs into another block.
    for (;;) {
        const int qrHeight = options.useQR ? QrBlockHeight(qrCell) : 0;
        const int barcodeBlock = options.useQR ? qrHeight : barcodeHeight;

        result.bannerTop = 0;
        int top = 0;
        if (result.showBanner) top = result.bannerHeight + result.gap;

        int bottom = heightDots;
        result.priceTop = bottom - result.priceHeight;
        bottom = result.priceTop;
        result.idTop = bottom;
        if (result.showId) {
            result.idTop = bottom - result.idHeight;
            bottom = result.idTop;
        }
        if (options.printBarcodeFirst) {
            result.barcodeTop = top;
            top += barcodeBlock + result.gap;
        } else {
            result.barcodeTop = bottom - barcodeBlock;
            bottom = result.barcodeTop;
        }

        result.nameTop = top;
        const int nameLines = (bottom - result.gap - top) / lineHeight;
        if (nameLines >= 1) {
            // A QR code is the one block that can give the description room
            // back without losing anything else: shrink its cells while the
            // name still needs more lines than are left, and stop at the
            // smallest cell that still scans (2 dots per module).
            if (options.useQR && qrCell > 2 && nameLines < desiredNameLines) {
                qrCell--;
                continue;
            }
            result.nameLines = nameLines;
            result.nameBlockHeight = nameLines * lineHeight;
            result.barcodeHeight = barcodeHeight;
            result.qrCell = options.useQR ? qrCell : 0;
            result.qrHeight = qrHeight;
            result.contentHeight = heightDots;
            return result;
        }

        if (result.showId) {
            result.showId = false;
            result.dropped.emplace_back("item id line hidden - label is too short");
            continue;
        }
        if (result.showPack) {
            result.showPack = false;
            result.dropped.emplace_back("pack description hidden - label is too short");
            continue;
        }
        if (result.showBanner) {
            result.showBanner = false;
            result.dropped.emplace_back("outlet banner hidden - label is too short");
            continue;
        }
        if (!options.useQR && barcodeHeight > 24) {
            barcodeHeight = std::max(24, barcodeHeight - 8);
            result.dropped.emplace_back("barcode height reduced to fit the label");
            continue;
        }
        if (options.useQR && qrCell > 2) {
            qrCell--;
            result.dropped.emplace_back("QR code cells reduced to fit the label");
            continue;
        }

        result.refused = true;
        result.refusalReason =
            "the label is too small for one line of the item name, the price and the barcode. "
            "Increase the label size in Label Printer Settings.";
        result.nameLines = 1;
        result.nameBlockHeight = lineHeight;
        result.barcodeHeight = barcodeHeight;
        result.qrCell = options.useQR ? qrCell : 0;
        result.qrHeight = options.useQR ? QrBlockHeight(qrCell) : 0;
        result.contentHeight = heightDots + 1;
        return result;
    }
}

Result ComputeDispensing(int widthDots, int heightDots, const Fonts& fonts, const Options& options) {
    Result result;
    if (widthDots <= 16 || heightDots <= 16) {
        result.refused = true;
        result.refusalReason = "the label is too small to print anything";
        return result;
    }

    result.gap = GapFor(widthDots, heightDots);
    result.bannerHeight = fonts.bannerHeight + 2;
    result.nameBlockHeight = static_cast<int>(std::lround(fonts.bodyHeight * 1.20));  // patient
    result.priceHeight = fonts.bodyHeight;                                           // quantity
    result.idHeight = fonts.tinyHeight;
    result.warningHeight = options.hasWarning ? fonts.bodyHeight * 2 : 0;
    result.poisonHeight = options.isPoison ? fonts.bodyHeight : 0;
    result.qtyWidth = std::max(60, static_cast<int>(std::lround(widthDots * 0.32)));
    result.showBanner = heightDots >= 28 * options.dotsPerMm;
    result.showDate = heightDots >= 24 * options.dotsPerMm;
    result.showExpiry = options.hasExpiry && heightDots >= 30 * options.dotsPerMm;

    int instructionLines = heightDots >= 40 * options.dotsPerMm ? 3 : 2;
    int barcodeHeight = ClampInt(static_cast<int>(std::lround(heightDots * 0.12)), 20, 48);
    const int lineHeight = fonts.bodyHeight + std::max(0, fonts.lineGap);

    for (;;) {
        const int total = result.gap * 8 +
                          (result.showBanner ? result.bannerHeight : 0) +
                          (result.showDate ? fonts.tinyHeight : 0) +
                          (result.showExpiry ? fonts.tinyHeight : 0) +
                          (options.hasPatient ? result.nameBlockHeight : 0) +
                          fonts.bodyHeight +  // item name
                          instructionLines * lineHeight +
                          result.warningHeight +
                          result.poisonHeight +
                          barcodeHeight +
                          result.priceHeight;
        if (total <= heightDots) {
            result.instructionLines = instructionLines;
            result.barcodeHeight = barcodeHeight;
            result.contentHeight = total;
            return result;
        }

        if (!result.showBanner && !result.showDate && !result.showExpiry && instructionLines == 1 &&
            barcodeHeight <= 20 &&
            (!options.hasPatient || result.nameBlockHeight == fonts.bodyHeight)) {
            result.refused = true;
            result.refusalReason =
                std::string("the label is too small for the mandatory dispensing text") +
                (options.hasWarning ? " including the dosage warning" : "") +
                (options.isPoison ? " and the controlled-drug line" : "") +
                ". Increase the label size in Label Printer Settings.";
            return result;
        }

        if (result.showBanner) {
            result.showBanner = false;
            result.dropped.emplace_back("outlet banner hidden - dispensing label is too short");
            continue;
        }
        if (result.showExpiry) {
            result.showExpiry = false;
            result.dropped.emplace_back("expiry date hidden - dispensing label is too short");
            continue;
        }
        if (result.showDate) {
            result.showDate = false;
            result.dropped.emplace_back("date hidden - dispensing label is too short");
            continue;
        }
        if (instructionLines > 1) {
            instructionLines--;
            result.dropped.emplace_back("dosage instruction reduced to fit the label");
            continue;
        }
        if (options.hasPatient && result.nameBlockHeight > fonts.bodyHeight) {
            result.nameBlockHeight = fonts.bodyHeight;
            result.dropped.emplace_back("patient name line reduced to fit the label");
            continue;
        }
        if (barcodeHeight > 20) {
            barcodeHeight = std::max(20, barcodeHeight - 4);
            result.dropped.emplace_back("barcode height reduced to fit the dispensing label");
            continue;
        }
    }
}

}  // namespace LabelFit
