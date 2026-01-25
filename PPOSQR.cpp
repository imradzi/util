#ifdef _WIN32
#include "winsock2.h"
#endif
#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#ifdef __clang__
#if __has_warning("-Wdeprecated-enum-enum-conversion")
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"  // warning: bitwise operation between different enumeration types ('XXXFlags_' and 'XXXFlagsPrivate_') is deprecated
#endif
#endif
#include "wx/wxprec.h"
#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif
#include "wx/rawbmp.h"
#include "wx/dcmemory.h"

#include <memory>
#include "PPOSQR.h"

using namespace PPOS;

using qrcodegen::QrCode;
using qrcodegen::QrSegment;
QR::QR(const std::string& text, const QrCode::Ecc level){
    qr = std::make_unique<QrCode>(QrCode::encodeText(text.c_str(), level));
}

QR::QR(const std::wstring& text, const QrCode::Ecc level) {
    auto utf8Text = boost::locale::conv::utf_to_utf<char>(text);
    qr = std::make_unique<QrCode>(QrCode::encodeText(utf8Text.c_str(), level));
}

std::string QR::getString(char boxChar) {
    std::string res;
    int border = 4;
    for (int y = -border; y < qr->getSize() + border; y++) {
        for (int x = -border; x < qr->getSize() + border; x++) {
            res.push_back(qr->getModule(x, y) ? boxChar : ' ');
        }
        res.append("\n");
    }
    res.append("\n");
    return res;
}

wxBitmap QR::getBitmap(wxBitmap bmp, int thickness, double ofsX, double ofsY, std::function<std::unique_ptr<wxDC>(double w, double h)> fnCreateDC) { // scale == 16 or 32 bit pixel
    auto sz = size();
    if (!bmp.IsOk()) bmp = wxBitmap(sz.GetWidth() * thickness, sz.GetWidth() * thickness);
    auto crDC = fnCreateDC ? fnCreateDC : [&](double w, double h) { return std::make_unique<wxMemoryDC>(bmp); };
    auto dc = crDC(sz.GetWidth(), sz.GetWidth());
    wxBrush oldbrush = dc->GetBrush();
    wxPen oldpen = dc->GetPen();
    dc->Clear();
    dc->SetBrush(*wxBLACK_BRUSH);
    draw([&](int x, int y) {
        dc->DrawRectangle(ofsX + x * thickness, ofsY + y * thickness, thickness, thickness);

    });
    dc->SetPen(oldpen);
    dc->SetBrush(oldbrush);
    return dc->GetAsBitmap();
}

wxBitmap QR::getBitmapCircular(wxBitmap bmp, int thickness, double ofsX, double ofsY, std::function<std::unique_ptr<wxDC>(double w, double h)> fnCreateDC) {  // scale == 16 or 32 bit pixel
    auto sz = size();
    auto radius = thickness / 2;
    if (!bmp.IsOk()) bmp = wxBitmap(sz.GetWidth() * thickness, sz.GetWidth() * thickness);
    auto crDC = fnCreateDC ? fnCreateDC : [&](double w, double h) { return std::make_unique<wxMemoryDC>(bmp); };
    auto dc = crDC(sz.GetWidth(), sz.GetWidth());
    wxBrush oldbrush = dc->GetBrush();
    wxPen oldpen = dc->GetPen();
    dc->Clear();
    dc->SetBrush(*wxBLACK_BRUSH);
    draw([&](int x, int y) {
        dc->DrawCircle(ofsX + x * thickness + radius, ofsY + y * thickness + radius, radius);
    });
    dc->SetPen(oldpen);
    dc->SetBrush(oldbrush);
    return dc->GetAsBitmap();
}

void QR::draw(std::function<void(int x, int y)> fnDraw) {
    int border = 4;
    for (int y = -border; y < qr->getSize() + border; y++) {
        for (int x = -border; x < qr->getSize() + border; x++) {
            if (qr->getModule(x, y)) fnDraw(x, y);
        }
    }
}
wxSize QR::size() {
    auto w = qr->getSize();
    return wxSize(w, w);
}

//------------------

//std::string printQR(const std::string &text) {
    //using qrcodegen::QrCode;
    //using qrcodegen::QrSegment;
    //const QrCode::Ecc errCorLvl = QrCode::Ecc::LOW;
    //auto qr = QrCode::encodeText(text.c_str(), errCorLvl);
    //std::string res;
    //int border = 4;
    //for (int y = -border; y < qr.getSize() + border; y++) {
    //    for (int x = -border; x < qr.getSize() + border; x++) {
    //        res.append(qr.getModule(x, y) ? "#" : " ");
    //    }
    //    res.append("\n");
    //}
    //res.append("\n");
    //return res;
//}

//void drawQR(const std::string &text, std::function<void(int x, int y)> fnDraw) {
    //using qrcodegen::QrCode;
    //using qrcodegen::QrSegment;
    //const QrCode::Ecc errCorLvl = QrCode::Ecc::LOW;
    //auto qr = QrCode::encodeText(text.c_str(), errCorLvl);
    //int border = 4;
    //for (int y = -border; y < qr.getSize() + border; y++) {
    //    for (int x = -border; x < qr.getSize() + border; x++) {
    //        if (qr.getModule(x, y)) fnDraw(x, y);
    //    }
    //}
//}


//wxSize getQRsize(const std::string &text) {
    //using qrcodegen::QrCode;
    //using qrcodegen::QrSegment;
    //const QrCode::Ecc errCorLvl = QrCode::Ecc::LOW;
    //auto qr = QrCode::encodeText(text.c_str(), errCorLvl);
    //auto w = qr.getSize();
    //return wxSize(w, w);
//}

// need to fix this. BMP file format
//wxBitmap createQRcode(const std::string text, double scale) {
    //using qrcodegen::QrCode;
    //using qrcodegen::QrSegment;

    //const QrCode::Ecc errCorLvl = QrCode::Ecc::LOW;
    //auto qr = QrCode::encodeText(text.c_str(), errCorLvl);
    //
    //const int size = qr.getSize() * scale;
    //const int byteWidth = (size + 7) / 8;
    //char *bitsChar = new char[size * byteWidth];

    //for (int y = 0; y < size; y++) {
    //    for (int xByte = 0; xByte < byteWidth; xByte++) {
    //        char bitChar = 0x00;
    //        if (!text.empty()) {
    //            for (int xBit = 0; xBit < 8; xBit++) {
    //                int x = xByte * 8 + xBit;
    //                int xModule = x / scale;
    //                int yModule = y / scale;
    //                bool black = qr.getModule(xModule, yModule);
    //                bitChar += black << (xBit % 8);
    //            }
    //        }
    //        bitsChar[y * byteWidth + xByte] = bitChar;
    //    }
    //}
    //auto retVal = std::make_unique<wxBitmap>(bitsChar, size, size, wxBITMAP_SCREEN_DEPTH);
    //delete[] bitsChar;
    //return retVal;
//}
