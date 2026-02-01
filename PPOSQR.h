#pragma once
#include <memory>
#include "qrcodegen.hpp"
class wxBitmap;
class wxSize;

namespace PPOS {
    class QR {
        std::string text;
        std::unique_ptr<qrcodegen::QrCode> qr {nullptr};

    public:
        QR(const std::string &text, const qrcodegen::QrCode::Ecc level = qrcodegen::QrCode::Ecc::LOW);
        QR(const std::wstring &text, const qrcodegen::QrCode::Ecc level = qrcodegen::QrCode::Ecc::LOW);
        std::string getString(char boxChar = '#');
        wxBitmap getBitmap(wxBitmap bmpBuffer, int thickness, double x=0, double y=0, std::function<std::unique_ptr<wxDC>(double w, double h)> fnCreateDC = nullptr);  // 16 or 32;
        wxBitmap getBitmapCircular(wxBitmap bmpBuffer, int thickness, double x = 0, double y = 0, std::function<std::unique_ptr<wxDC>(double w, double h)> fnCreateDC = nullptr);
        std::string getPNGBase64(int scale = 4, int border = 4, bool circular = false);  // Returns PNG as base64 string, no wxWidgets dependency
        void draw(std::function<void(int x, int y)> fnDraw);
        wxSize size();
        int getHeight() { return qr->getSize(); }
        int getWidth() { return qr->getSize(); }
        static std::string getPNGBase64(const std::string &text, int scale = 4, int border = 4, bool circular = false, const qrcodegen::QrCode::Ecc level = qrcodegen::QrCode::Ecc::LOW) {
            QR qr(text, level);
            return qr.getPNGBase64(scale, border, circular);
        }
    };
}
