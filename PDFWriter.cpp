#include "precompiled/libcommon.h"
#ifdef _WIN32
#include "winsock2.h"
#endif
#include "expression.h"

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

#include "wx/numformatter.h"

#include <list>
#include <memory>
#include <filesystem>

#include "wpObject.h"

#ifndef NO_XLS
#include "ExcelReader.h"
#endif
#include "PDFWriter.h"
#include "wx/pdfbarcode.h"
#include "wx/pdfdc.h"
#include "PPOSQR.h"
#include "e-invoice/e-invoice.h"
#include <cstring>

double ReportPDF::GetFontHeight() const { return GetFontSize() * 0.6; }

ReportPDF::ReportPDF(const std::wstring &title, const std::wstring _outletName, int orientation) : wxPdfDocument(orientation),
                                                                                                 outletName(_outletName),
                                                                                                 _title(title),
                                                                                                 colDef(NULL),
                                                                                                 formatter(NULL),
                                                                                                 showFooter(true),
                                                                                                 breakPageOn(false),
                                                                                                 pageOrientation(orientation),
                                                                                                 showFooterPageNo(true) {
    fontSize.title = 20;
    fontSize.label = 8;
    fontSize.data = 8;
    fontSize.total = 7;
    fontSize.note = 7;
    fontSize.footer = 6;

    fnIsLastLine = []() { return false; };
    firstPagePrinted = false;
    SetTitle(title);
    x0 = GetLeftMargin();
    xR = GetRightMargin();
    y0 = GetTopMargin();
    w0 = GetPageWidth();
    h0 = GetPageHeight();
    SetLineWidth(0.1);
}

ReportPDF::~ReportPDF() {
    if (formatter)
        delete formatter;
}

void ReportPDF::ComputeColumnWeightage(std::vector<PDFColumnDefinition> *columnDefs, int tabStart) {
    if (!columnDefs) return;
    double s = 0;
    for (auto const &it : *columnDefs) {
        s += it.weightage;
    }
    double pw = (GetPageWidth() - tabStart) - GetRightMargin() * 2;

    for (auto &&cd : *columnDefs) {
        cd.w = pw * cd.weightage / s;
    }
}

void ReportPDF::RenderData(std::vector<PDFColumnDefinition> *p, wpSQLResultSet &rs) {
    if (!p) return;
    if (int(p->size()) != rs.GetColumnCount())
        throw std::runtime_error("column defs count not same as resultset columns");
    bool isLastLine = fnIsLastLine();
    const double height = GetFontHeight();
    for (int i = 0; i < rs.GetColumnCount(); i++) {
        PDFColumnDefinition &cd = p->at(i);
        if (cd.show)
            ClippedCell(cd.w, height, rs.Get<std::wstring>(i), cd.border | (isLastLine ? lastLineBorder : 0), 0, cd.align);
    }
}

void ReportPDF::RenderData(std::vector<PDFColumnDefinition> *p, const std::vector<std::wstring> &data) {
    if (!p) return;
    if (p->size() != data.size())
        throw std::runtime_error("column defs count not same as string data columns");
    const double height = GetFontHeight();
    std::vector<PDFColumnDefinition>::const_iterator idef = p->begin();
    bool isLastLine = fnIsLastLine();
    for (auto const &it : data) {
        const PDFColumnDefinition &cd = *idef;
        if (cd.show)
            ClippedCell(cd.w, height, it, cd.border | (isLastLine ? lastLineBorder : 0), 0, cd.align);
        idef++;
    }
}

void ReportPDF::Header() {
    firstPagePrinted = true;
    if (formatter) {
        SetFont("Arial", "", 10);
        double height = GetFontHeight();
        Cell(w0 - 50, height, _title, wxPDF_BORDER_NONE, 0, wxPDF_ALIGN_LEFT);
        Cell(0, 5, wxDateTime::Now().Format("%d-%m-%Y %a %X"), wxPDF_BORDER_NONE, 1, wxPDF_ALIGN_RIGHT);
        Cell(w0 - 50, height, subTitle, wxPDF_BORDER_NONE, 0, wxPDF_ALIGN_LEFT);
        Cell(0, 5, "Page: " + String::IntToString(PageNo()), wxPDF_BORDER_NONE, 1, wxPDF_ALIGN_RIGHT);
        if (breakPageOn) {
            Line(x0, GetY(), GetPageWidth() - x0, GetY());
            Ln(GetFontHeight() / 4.0);
            wxStringTokenizer tk(pageTitle, "\n");
            while (tk.HasMoreTokens()) {
                std::wstring s(tk.GetNextToken());
                Cell(0, GetFontHeight(), s, wxPDF_BORDER_NONE, 1, wxPDF_ALIGN_LEFT);
                SetFont("Arial", "", 8);
            }
        }
        SetFont("Arial", "B", 6);
        height = GetFontHeight();
        for (size_t i = (breakPageOn ? 1 : 0); i < formatter->def().size(); i++) {
            auto &cdef = *formatter->def()[i];
            ClippedCell(cdef.size, height, cdef.colName, wxPDF_BORDER_TOP | wxPDF_BORDER_BOTTOM | wxPDF_BORDER_LEFT | wxPDF_BORDER_RIGHT);
            cdef.pageTotal = 0;
            cdef.nRecPage = 0;
        }
        Ln();
        SetFont("Arial", "", 7);
    } else if (colDef)
        RenderHeader(colDef);
}

void ReportPDF::RenderHeader(std::vector<PDFColumnDefinition> *cdef, const std::wstring addLine) {
    wxPdfColour c = GetFillColour();
    SetFont("Arial", "", fontSize.label);
    if (fillRect) {
        SetFillColour(fillColorRed, fillColorBlue, fillColorGreen);
    }
    SetDrawColour(boxColorRed, boxColorBlue, boxColorGreen);
    const double height = GetFontHeight();
    if (!addLine.empty()) {
        double w {0};
        for (size_t i = 0; i < cdef->size(); i++) {
            w += cdef->at(i).w;
        }
        ClippedCell(w, height, addLine, wxPDF_BORDER_NONE, 0, wxPDF_ALIGN_RIGHT, fillRect ? 1 : 0);
        Ln();
    }
    for (size_t i = 0; i < cdef->size(); i++) {
        PDFColumnDefinition &cd = cdef->at(i);
        if (cd.w <= 0) continue;
        ClippedCell(cd.w, height, cd.title, wxPDF_BORDER_TOP | (i == 0 ? wxPDF_BORDER_LEFT : 0) | wxPDF_BORDER_RIGHT | wxPDF_BORDER_BOTTOM, 0, 0, fillRect ? 1 : 0);
    }
    Ln();
    SetFillColour(c);
    SetFont("Arial", "", fontSize.data);
}

void ReportPDF::Footer() {
    if (!showFooter) return;
    if (formatter) {
        bool hasSubtotal = false;
        bool hasExpression = false;
        int nCol = formatter->def().size();
        for (int i = 0; i < nCol; i++) {
            DB::XLSColumnFormatter::ColumnDefinition &cdef = *formatter->def()[i];
            if (!cdef.sumFunction.empty()) {
                hasSubtotal = true;
            }
            if (cdef.footerExpression) {
                hasExpression = true;
            }
        }

        if (!hasSubtotal) return;

        SetFont("Arial", "B", fontSize.total);
        const double height = GetFontHeight();
        double *vec = NULL;
        if (hasExpression) {
            vec = new double[nCol];
            memset(vec, 0, sizeof(double) * nCol);
            for (int i = 0; i < nCol; i++) {
                DB::XLSColumnFormatter::ColumnDefinition &cdef = *formatter->def()[i];
                if (cdef.type == DB::XLSColumnFormatter::Number)
                    vec[i] = cdef.pageTotal;
            }
        }
        for (int i = (breakPageOn ? 1 : 0); i < nCol; i++) {
            DB::XLSColumnFormatter::ColumnDefinition &cdef = *formatter->def()[i];
            std::wstring v;
            if (cdef.subTotalLabel)
                ClippedCell(cdef.size, height, "TOTAL THIS PAGE", wxPDF_BORDER_TOP | wxPDF_BORDER_BOTTOM | wxPDF_BORDER_LEFT | wxPDF_BORDER_RIGHT, 0, wxPDF_ALIGN_RIGHT);
            else if (cdef.type == DB::XLSColumnFormatter::Number) {
                if (boost::iequals(cdef.sumFunction, "sum"))
                    v = wxNumberFormatter::ToString(cdef.pageTotal, cdef.precision);
                else if (boost::iequals(cdef.sumFunction, "average") || boost::iequals(cdef.sumFunction, "avg"))
                    v = wxNumberFormatter::ToString((cdef.nRecPage != 0 ? cdef.pageTotal / cdef.nRecPage : 0.0), cdef.precision);
                else if (cdef.footerExpression) {
                    DB::SetVector(cdef.footerExpression.get(), vec);
                    v = wxNumberFormatter::ToString(DB::ExecuteExpression(cdef.footerExpression.get()), cdef.precision);
                }
                ClippedCell(cdef.size, height, v, wxPDF_BORDER_TOP | wxPDF_BORDER_BOTTOM | wxPDF_BORDER_LEFT | wxPDF_BORDER_RIGHT, 0, wxPDF_ALIGN_RIGHT);
            } else
                ClippedCell(cdef.size, height, v, wxPDF_BORDER_TOP | wxPDF_BORDER_BOTTOM | wxPDF_BORDER_LEFT | wxPDF_BORDER_RIGHT, 0, wxPDF_ALIGN_RIGHT);
        }
        if (vec) delete[] vec;
    } else if (showFooterPageNo) {
        Cell(1, 6, "", wxPDF_BORDER_TOP);
        SetY(-15);  // Position at 1.5 cm from bottom
        SetFont("Arial", "I", fontSize.footer);
        const double height = GetFontHeight();
        Cell(0, height, wxString::Format("Page %d", PageNo()), 0, 0, wxPDF_ALIGN_RIGHT);  // Page number
    }
    auto _y = GetY();
    SetY(-5);  // 1cm from bottom;
    SetFont("Arial", "", 6);
    Cell(0, 0, outletName, 0, 0, wxPDF_ALIGN_LEFT);
    SetY(_y);
    Ln(GetFontHeight());
}

void ReportPDF::ShowGrandTotal() {
    if (formatter) {
        bool hasSubtotal = false;
        bool hasExpression = false;
        int nCol = formatter->def().size();
        for (int i = 0; i < nCol; i++) {
            DB::XLSColumnFormatter::ColumnDefinition &cdef = *formatter->def()[i];
            if (!cdef.sumFunction.empty()) {
                hasSubtotal = true;
            }
            if (cdef.footerExpression) {
                hasExpression = true;
            }
        }

        if (!hasSubtotal) return;

        double *vec = NULL;
        if (hasExpression) {
            vec = new double[nCol];
            memset(vec, 0, sizeof(double) * nCol);
            for (int i = 0; i < nCol; i++) {
                DB::XLSColumnFormatter::ColumnDefinition &cdef = *formatter->def()[i];
                if (cdef.type == DB::XLSColumnFormatter::Number)
                    vec[i] = cdef.grandTotal;
            }
        }

        SetFont("Arial", "B", fontSize.total);
        const double height = GetFontHeight();
        for (int i = (breakPageOn ? 1 : 0); i < nCol; i++) {
            DB::XLSColumnFormatter::ColumnDefinition &cdef = *formatter->def()[i];
            std::wstring v;
            if (cdef.subTotalLabel)
                ClippedCell(cdef.size, height, "GRAND TOTAL", wxPDF_BORDER_TOP | wxPDF_BORDER_BOTTOM | wxPDF_BORDER_LEFT | wxPDF_BORDER_RIGHT, 0, wxPDF_ALIGN_RIGHT);
            else if (cdef.type == DB::XLSColumnFormatter::Number) {
                if (boost::iequals(cdef.sumFunction, "sum"))
                    v = wxNumberFormatter::ToString(cdef.grandTotal, cdef.precision);
                else if (boost::iequals(cdef.sumFunction, "average") || boost::iequals(cdef.sumFunction, "avg"))
                    v = wxNumberFormatter::ToString((cdef.nRecTotal != 0 ? cdef.grandTotal / cdef.nRecTotal : 0), cdef.precision);
                else if (cdef.footerExpression) {
                    DB::SetVector(cdef.footerExpression.get(), vec);
                    v = wxNumberFormatter::ToString(DB::ExecuteExpression(cdef.footerExpression.get()), cdef.precision);
                }
                ClippedCell(cdef.size, height, v, wxPDF_BORDER_TOP | wxPDF_BORDER_BOTTOM | wxPDF_BORDER_LEFT | wxPDF_BORDER_RIGHT, 0, wxPDF_ALIGN_RIGHT);
            } else
                ClippedCell(cdef.size, height, v, wxPDF_BORDER_TOP | wxPDF_BORDER_BOTTOM | wxPDF_BORDER_LEFT | wxPDF_BORDER_RIGHT, 0, wxPDF_ALIGN_RIGHT);
        }
        Ln();
        SetFont("Arial", "", fontSize.data);
        if (vec) delete[] vec;
    }
}
std::tuple<double, int> ReportPDF::WriteBulletPoints(double x, double y, double w, double lineHeight, const std::wstring &title, std::vector<std::wstring> &points, double h) {
    double m = GetLeftMargin();
    SetXY(x, y);
    SetLeftMargin(x);
    wxPdfColour c = GetFillColour();
    if (fillRect) SetFillColour(fillColorRed, fillColorBlue, fillColorGreen);  // label color
    SetDrawColour(boxColorRed, boxColorBlue, boxColorGreen);
    const double oldLH = GetLineHeight();
    SetLineHeight(lineHeight);
    int nLines = 0;
    Cell(w, lineHeight, title, wxPDF_BORDER_FRAME, 0, 0, fillRect ? 1 : 0);
    Ln(lineHeight * 1.2);
    nLines++;
    int i = 1;
    int pointWidth = 5;
    for (auto const &it : points) {
        if (!String::IsEmpty(it)) {
            Cell(pointWidth, lineHeight, fmt::format("{:d}.", i), wxPDF_BORDER_NONE, 0, wxPDF_ALIGN_RIGHT);
            MultiCell(w - pointWidth, lineHeight, it, wxPDF_BORDER_NONE, wxPDF_ALIGN_LEFT);
            int n = std::count_if(it.begin(), it.end(), [](const char &e) { return e == 0x0A; });
            if (n <= 1)
                nLines++;
            else
                nLines += n;

        } else {
            Cell(pointWidth, lineHeight, "", wxPDF_BORDER_NONE, 1, wxPDF_ALIGN_RIGHT);
            nLines++;
        }
        i++;
    }
    SetLeftMargin(m);

    double height = GetY() - y + lineHeight;

    if (h > 0) height = h;

    Rect(x, y, w, height);  // +lineHeight / 2);
    // Line(x, y+lineHeight, x+w, y+lineHeight);
    SetFillColour(c);
    SetLineHeight(oldLH);
    return {height, nLines};
}

std::tuple<double, int> ReportPDF::WriteList(double x, double y, double w, double lineHeight, const std::wstring &title, std::vector<std::wstring> &points, double h) {
    double m = GetLeftMargin();
    wxPdfColour c = GetFillColour();
    SetXY(x, y);
    SetLeftMargin(x);
    // SetFillColour(0xE0, 0xE0, 0xE0);
    if (fillRect) {
        SetFillColour(fillColorRed, fillColorBlue, fillColorGreen);
    }
    SetDrawColour(boxColorRed, boxColorBlue, boxColorGreen);

    const double oldLH = GetLineHeight();
    SetLineHeight(lineHeight);
    int nLines = 0;
    Cell(w, lineHeight, title, wxPDF_BORDER_FRAME, 0, 0, fillRect ? 1 : 0);
    Ln(lineHeight * 1.2);
    nLines++;
    int i = 1;
    int pointWidth = 5;
    for (auto const &it : points) {
        Cell(pointWidth, lineHeight, "", wxPDF_BORDER_NONE, 0, wxPDF_ALIGN_RIGHT);
        MultiCell(w - pointWidth, lineHeight, it, wxPDF_BORDER_NONE, wxPDF_ALIGN_LEFT);
        i++;
        auto n = std::count_if(it.begin(), it.end(), [](const char &e) { return e == 0x0A; });
        if (n > 1)
            nLines += n;
        else
            nLines++;
    }
    SetLeftMargin(m);
    SetFillColour(c);
    double height = GetY() - y + lineHeight;

    if (h > 0) height = h;

    Rect(x, y, w, height + lineHeight / 2);
    // Line(x, y+lineHeight, x+w, y+lineHeight);
    SetLineHeight(oldLH);
    return {GetY(), nLines};
}

wxBitmap ReportPDF::createQRBitmap(double ofsX, double ofsY, const std::wstring qrString) {
    auto constexpr thickness = 3.0;
    auto qr = std::make_unique<PPOS::QR>(qrString);
    auto pw = GetPageWidth();
    auto ph = GetPageHeight();
    wxBitmap bmp(pw, ph);
    if (qr) return qr->getBitmapCircular(bmp, thickness, ofsX, ofsY, [this, pw, ph](double w, double h) {
        auto dc = std::make_unique<wxPdfDC>(this, pw, ph);
        //auto mode = dc->GetMapMode();
        return dc;
    });
    return wxBitmap();
}

std::tuple<double, int> ReportPDF::WriteLetterHead(const std::wstring &companyName, const std::wstring &companyName2, const std::wstring &address, const std::wstring &regNo, const std::wstring &imageFileName, const std::wstring &eInvoiceQRstring) {
    double lm = GetLeftMargin();
    double rm = GetRightMargin();
    double startAfterImage = 0;
    const double height = GetFontHeight();
    int nLines = 0;
    if (!imageFileName.empty()) {
        if (std::filesystem::exists(imageFileName)) {
            Image(imageFileName, x0, y0);
            auto lastX = GetLastImageBottomRightX();
            auto lastY = GetLastImageBottomRightY();
            SetLeftMargin(lastX + 2);
            SetRightMargin(x0 / 2);
            SetXY(lastX + 2, y0);
            startAfterImage = lastY + 5;
            nLines += 10;
        } else {
            //bitmap = createQRBitmap(x0, y0, companyName + L", " + companyName2 + L"\n" + address);
            //if (bitmap.IsOk()) Image("logo", bitmap.ConvertToImage(), x0, y0);
            auto lastX = GetLastImageBottomRightX();
            auto lastY = GetLastImageBottomRightY();
            SetLeftMargin(lastX + 2);
            SetRightMargin(x0 / 2);
            SetXY(lastX + 2, y0);
            startAfterImage = lastY + 5;
            nLines += 5;
        }
    } else {
        SetLeftMargin(x0 / 2);
        SetRightMargin(x0 / 2);
    }
    if (LHDN::isValidQR(eInvoiceQRstring)) {
        SetDrawColour(boxColorRed, boxColorBlue, boxColorGreen);
        SetFont("Arial", "", 8);
        SetLeftMargin(x0 / 2);
        SetRightMargin(x0 / 2);
        constexpr auto ttl = L"LHDN e-invoice";
        ShowLog(fmt::format("printing QR: {}", to_string(eInvoiceQRstring)));
        SetXY(x0+110, y0);
        Cell(0, 0, ttl, wxPDF_BORDER_NONE, 0, 0, 0);
        createQRBitmap(x0+100, y0+5, eInvoiceQRstring);
        // wxBitmap bitmap = createQRBitmap(eInvoiceQRstring);
        // if (bitmap.IsOk()) {
        //     Image("eInvoice", bitmap.ConvertToImage(), x0+110, y0);
        //     if (nLines == 0) nLines += 5;
        // } else {
        //     ShowLog("****> printing QR: Bitmap NOT OK");
        // }
    }
    wxPdfColour origColor = GetFillColour();
    if (letterheadFillRect) {
        SetFillColour(fillColorRed, fillColorBlue, fillColorGreen);
    }
    SetDrawColour(boxColorRed, boxColorBlue, boxColorGreen);

    //SetFont("Arial", "", 10);
    //Cell(0, height, "", wxPDF_BORDER_NONE, 1, 0, letterheadFillRect ? 1 : 0);

    SetFont("Arial", "", 20);
    auto x = GetX();
    SetX(x0/2);
    Cell(0, height, "", wxPDF_BORDER_NONE, 0, 0, letterheadFillRect ? 1 : 0);  // a buffer
    SetX(x);
    Cell(GetStringWidth(companyName) + GetCellMargin(), height, companyName, wxPDF_BORDER_NONE, 0, 0, letterheadFillRect ? 1 : 0);
    SetFont("Arial", "", 10);
    if (!regNo.empty())
        Cell(0, height, " (" + String::to_string(regNo) + ")", wxPDF_BORDER_NONE, 0, 0, letterheadFillRect ? 1 : 0);
    else
        Cell(0, height, " ", wxPDF_BORDER_NONE, 0, 0, letterheadFillRect ? 1 : 0);

    Ln();
    if (!companyName2.empty()) {
        Cell(5, height, "", wxPDF_BORDER_NONE, 0, 0, letterheadFillRect ? 1 : 0);  // a buffer
        Cell(0, height, companyName2, wxPDF_BORDER_NONE, 1, 0, letterheadFillRect ? 1 : 0);
    }

    SetFont("Arial", "", 7);
    wxStringTokenizer tok(address, "\n");
    while (tok.HasMoreTokens()) {
        nLines++;
        std::wstring addr(tok.GetNextToken().Trim().Trim(false));
        Cell(5, height / 2, "", wxPDF_BORDER_NONE, 0, 0, letterheadFillRect ? 1 : 0);  // a buffer
        Cell(0, height / 2, addr, wxPDF_BORDER_NONE, 1, 0, letterheadFillRect ? 1 : 0);
    }
    Cell(0, height, "", wxPDF_BORDER_NONE, 1, 0, letterheadFillRect ? 1 : 0);  // a buffer
    SetFillColour(origColor);
    SetLeftMargin(lm);
    SetRightMargin(rm);
    if (GetY() < startAfterImage && startAfterImage > 0) SetY(startAfterImage);

    return {GetY() + GetLineHeight(), nLines};
}

void ReportPDF::DrawFullLine() {
    Line(5, GetY(), w0 - 5, GetY());
}

void ReportPDF::WriteBox(double x, double y, const std::wstring &title, const std::vector<std::wstring> &lines, double rightAlignTo) {
    double lm = GetLeftMargin();
    int align = wxPDF_ALIGN_LEFT;
    if (rightAlignTo > 0) align = wxPDF_ALIGN_RIGHT;
    SetXY(x, y);
    SetLeftMargin(x);
    // Write(6, title);
    SetFont("Arial", "", 12);
    double height = GetFontHeight() * 0.7;
    Cell(0, height, title, 0, 1, align);
    SetFont("Arial", "", 8);
    height = GetFontHeight() * 0.7;
    for (auto const &it : lines) {
        Cell(0, height, it, 0, 1, align);
    }
    SetLeftMargin(lm);
}
