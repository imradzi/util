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
#include <vector>
#include <zlib.h>
#include <boost/locale.hpp>
#include "PPOSQR.h"
#include "encrypt64.h"

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

// Helper to write big-endian 32-bit value
static void writeBE32(std::vector<uint8_t>& out, uint32_t val) {
    out.push_back((val >> 24) & 0xFF);
    out.push_back((val >> 16) & 0xFF);
    out.push_back((val >> 8) & 0xFF);
    out.push_back(val & 0xFF);
}

// Helper to write PNG chunk
static void writePNGChunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    writeBE32(out, static_cast<uint32_t>(data.size()));
    size_t typeStart = out.size();
    for (int i = 0; i < 4; i++) out.push_back(type[i]);
    out.insert(out.end(), data.begin(), data.end());
    // CRC32 over type + data
    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, out.data() + typeStart, 4 + data.size());
    writeBE32(out, crc);
}

std::string QR::getPNGBase64(int scale, int border, bool circular) {
    int qrSize = qr->getSize();
    int imgSize = (qrSize + border * 2) * scale;
    double radius = scale / 2.0;
    double radiusSq = radius * radius;
    
    // Create raw image data (1 byte per pixel: 0=black, 255=white)
    std::vector<uint8_t> rawData;
    rawData.reserve(imgSize * (imgSize + 1));  // +1 for filter byte per row
    
    for (int py = 0; py < imgSize; py++) {
        rawData.push_back(0);  // PNG filter byte (None)
        for (int px = 0; px < imgSize; px++) {
            int qx = px / scale - border;
            int qy = py / scale - border;
            bool isBlack = false;
            
            if (qx >= 0 && qx < qrSize && qy >= 0 && qy < qrSize && qr->getModule(qx, qy)) {
                if (circular) {
                    // Calculate distance from center of this QR module
                    double centerX = (qx + border) * scale + radius;
                    double centerY = (qy + border) * scale + radius;
                    double dx = px - centerX + 0.5;  // +0.5 for pixel center
                    double dy = py - centerY + 0.5;
                    isBlack = (dx * dx + dy * dy) <= radiusSq;
                } else {
                    isBlack = true;
                }
            }
            rawData.push_back(isBlack ? 0 : 255);
        }
    }
    
    // Compress with zlib (deflate)
    uLongf compressedSize = compressBound(rawData.size());
    std::vector<uint8_t> compressed(compressedSize);
    compress2(compressed.data(), &compressedSize, rawData.data(), rawData.size(), Z_BEST_COMPRESSION);
    compressed.resize(compressedSize);
    
    // Build PNG file
    std::vector<uint8_t> png;
    
    // PNG signature
    const uint8_t signature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    png.insert(png.end(), signature, signature + 8);
    
    // IHDR chunk
    std::vector<uint8_t> ihdr;
    writeBE32(ihdr, imgSize);  // width
    writeBE32(ihdr, imgSize);  // height
    ihdr.push_back(8);         // bit depth
    ihdr.push_back(0);         // color type (grayscale)
    ihdr.push_back(0);         // compression method
    ihdr.push_back(0);         // filter method
    ihdr.push_back(0);         // interlace method
    writePNGChunk(png, "IHDR", ihdr);
    
    // IDAT chunk (compressed image data wrapped in zlib format)
    // Note: compress2 already produces zlib-format data
    writePNGChunk(png, "IDAT", compressed);
    
    // IEND chunk
    writePNGChunk(png, "IEND", {});
    
    // Convert to base64
    std::string pngStr(png.begin(), png.end());
    return UsingBoost::encode(pngStr);
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
