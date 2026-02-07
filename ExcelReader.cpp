#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
#ifdef _WIN32
#include "winsock2.h"
#endif

#ifdef __clang__
#if __has_warning("-Wdeprecated-enum-enum-conversion")
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"  // warning: bitwise operation between different enumeration types ('XXXFlags_' and 'XXXFlagsPrivate_') is deprecated
#endif
#endif

//#include "wx/wxprec.h"
//
//#ifndef WX_PRECOMP
//#include "wx/wx.h"
//#endif

#ifndef NO_XLS
//#include "global.h"
//#include "words.h"
#include <chrono>
#include <boost/algorithm/string.hpp>
#include <fmt/xchar.h>
#include <fmt/format.h>
#include <string_view>
#include "ExcelReader.h"

#ifdef _WIN32
auto libxl_regName = L"Mohd Radzi Ibrahim";
auto libxl_regKey = L"windows-232c2f0d03cce50067b26e6ca4e1dfj3";  // starting 17 Jan 2013
#else
auto libxl_regName = L"Mohd Radzi Ibrahim";
auto libxl_regKey = L"linux-e3dc1f7d93aca51007022e3c44q1efu3";  // starting 19 Dec 2014
#endif

static std::wstring TrimDecimal(std::wstring newStr) {
    if (!boost::contains(newStr, ".")) return newStr;
    size_t l = newStr.length();
    size_t newLength = l;
    bool toTruncate = false;
    for (; l > 0; l--) {
        auto v = newStr[l - 1];
        if (v == '0') {
            newLength = l;
            toTruncate = true;
        } else if (std::isdigit(v)) {
            newLength = l;
            break;
        } else if (v == '.') {
            newLength = l - 1;
            toTruncate = true;
            break;
        }
    }
    if (toTruncate) newStr.resize(newLength);
    return newStr;
}

ExcelReader::ExcelReader(bool isXLSX) : sheet(nullptr) {
    book = CreateBook(isXLSX);
#ifdef WIN32
    twoDigitFormat = book->addCustomNumFormat(L"#0.00");
#else
    twoDigitFormat = book->addCustomNumFormat(L"#0.00");
#endif
    fmtNum = book->addFormat();
    fmtNum->setNumFormat(libxl::NUMFORMAT_NUMBER_SEP);
    fmtNum->setAlignH(libxl::ALIGNH_RIGHT);
    fmtNum->setAlignV(libxl::ALIGNV_CENTER);
    fmtNumDec = book->addFormat();
    fmtNumDec->setNumFormat(libxl::NUMFORMAT_NUMBER_SEP_D2);
    fmtNumDec->setAlignH(libxl::ALIGNH_RIGHT);
    fmtNumDec->setAlignV(libxl::ALIGNV_CENTER);
    fmtCenter = book->addFormat();
    fmtCenter->setAlignH(libxl::ALIGNH_CENTER);
    fmtCenter->setAlignV(libxl::ALIGNV_CENTER);
    fmtRight = book->addFormat();
    fmtRight->setAlignH(libxl::ALIGNH_RIGHT);
    fmtRight->setAlignV(libxl::ALIGNV_CENTER);
    fmt = nullptr;
    font = book->addFont();
    font->setSize(20);
    fontHalf = book->addFont();
    fontHalf->setSize(15);
}

libxl::Book *ExcelReader::CreateBook(bool isXLSX) {
    libxl::Book *book = isXLSX ? xlCreateXMLBook() : xlCreateBook();
    if (!book) {
        throw std::runtime_error("Cannot create excel bool");
    }
    book->setKey(libxl_regName, libxl_regKey);
    return book;
}

ExcelReader::~ExcelReader() {
    if (book) book->release();
}

bool ExcelReader::Open(const std::wstring &filename) { return book->load(filename.c_str()); }

std::wstring ExcelReader::UnpackDate(double x) {
    int yr, mth, day, hr, min, sec, msec;
    if (book->dateUnpack(x, &yr, &mth, &day, &hr, &min, &sec, &msec)) {
        auto dt = std::chrono::system_clock::now();
        auto n = time(nullptr);
        auto lt = localtime(&n);
        lt->tm_hour = hr;
        lt->tm_min = min;
        lt->tm_sec = sec;
        if (day > 0 && mth >= 0 && yr > 0) {
            lt->tm_mday = day;
            lt->tm_mon = mth - 1;
            lt->tm_year = yr;
        }
        dt = std::chrono::system_clock::from_time_t(mktime(lt));
        dt += std::chrono::milliseconds(msec);
        return std::to_wstring(dt.time_since_epoch().count());
    }
    return TrimDecimal(fmt::format(L"{:.5f}", x));
}

std::chrono::system_clock::time_point ExcelReader::UnpackDateToChrono(double x) {
    int yr, mth, day, hr, min, sec, msec;
    if (book->dateUnpack(x, &yr, &mth, &day, &hr, &min, &sec, &msec)) {
        auto dt = std::chrono::system_clock::now();
        auto n = time(nullptr);
        auto lt = localtime(&n);
        lt->tm_hour = hr;
        lt->tm_min = min;
        lt->tm_sec = sec;
        if (day > 0 && mth >= 0 && yr > 0) {
            lt->tm_mday = day;
            lt->tm_mon = mth - 1;
            lt->tm_year = yr;
        }
        dt = std::chrono::system_clock::from_time_t(mktime(lt));
        dt += std::chrono::milliseconds(msec);
        return dt;
    }
    return std::chrono::system_clock::time_point();
}

bool ExcelReader::ReadSheet(std::vector<std::vector<std::wstring>> &data, const std::wstring &sheetName) {  // if sheetname is blank, then read active sheet;
    if (sheetName.empty())
        return ReadSheet(data, book->activeSheet());
    else {
        for (int i = 0; i < book->sheetCount(); i++) {
            sheet = book->getSheet(i);
            auto name = sheet->name();
            if (boost::iequals(name, sheetName))
                return ReadSheetData(data);
        }
    }
    return false;
}

bool ExcelReader::ReadSheet(std::vector<std::vector<std::wstring>> &data, int sheetIndex) {
    sheet = book->getSheet(sheetIndex);
    return ReadSheetData(data);
}

std::wstring ExcelReader::ReadCell(libxl::Sheet *sh, int row, int col) {
    switch (sh->cellType(row, col)) {
        case libxl::CELLTYPE_NUMBER: {
            libxl::Format *fmtLocal;
            double x = sh->readNum(row, col, &fmtLocal);
            if (fmtLocal->numFormat() == libxl::NUMFORMAT_DATE
                || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_D_MON_YY
                || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_D_MON
                || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_MON_YY
                || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_HMM_AM
                || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_HMMSS_AM
                || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_HMM
                || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_HMMSS
                || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_MDYYYY_HMM) {
                return UnpackDate(x);
            } else {
                return TrimDecimal(fmt::format(L"{:.5f}", x));
            }
            break;
        }
        case libxl::CELLTYPE_STRING: {
            return sh->readStr(row, col);
        }
        case libxl::CELLTYPE_BLANK:
        case libxl::CELLTYPE_EMPTY: {
            return L"";
        }
        default: {
            return sh->readStr(row, col);
        }
    }
    return L"";
}

void ExcelReader::WriteCell(libxl::Sheet* sh, int row, int col, const std::wstring& v, bool isNumeric, bool isRightJustified) const {
    if (isNumeric) {
        auto n = boost::replace_all_copy(v, ",", "");
        if (n.empty()) sh->writeNum(row, col, 0.0, fmtNumDec);
        else sh->writeNum(row, col, std::stod(n), fmtNumDec);
    } else {
        sh->writeStr(row, col, v.c_str());
        if (isRightJustified) sh->setCellFormat(row, col, fmtRight);
    }
}

std::wstring ExcelReader::ReadCellDate(libxl::Sheet *sh, int row, int col) {
    libxl::Format *fmtLocal;
    double x = sh->readNum(row, col, &fmtLocal);
    return UnpackDate(x);
}

bool ExcelReader::ReadSheetData(std::vector<std::vector<std::wstring>> &data) {
    if (!sheet) {
        return false;
    }
    for (long row = sheet->firstRow(); row < sheet->lastRow(); row++) {
        auto &rowData = data.emplace_back();
        for (long col = sheet->firstCol(); col < sheet->lastCol(); col++) {
#ifdef _DEBUG
            std::wstring sdata = sheet->readStr(row, col);
#endif
            switch (sheet->cellType(row, col)) {
                case libxl::CELLTYPE_NUMBER: {
                    libxl::Format *fmtLocal;
                    double x = sheet->readNum(row, col, &fmtLocal);
                    if (fmtLocal->numFormat() == libxl::NUMFORMAT_DATE
                        || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_D_MON_YY
                        || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_D_MON
                        || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_MON_YY
                        || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_HMM_AM
                        || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_HMMSS_AM
                        || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_HMM
                        || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_HMMSS
                        || fmtLocal->numFormat() == libxl::NUMFORMAT_CUSTOM_MDYYYY_HMM) {
                        rowData.emplace_back(UnpackDate(x));
                    } else
                        rowData.emplace_back(TrimDecimal(fmt::format(L"{:.5f}", x)));
                    break;
                }
                case libxl::CELLTYPE_STRING: {
                    auto t = sheet->readStr(row, col);
                    rowData.emplace_back(t);
                    break;
                }
                case libxl::CELLTYPE_BLANK:
                case libxl::CELLTYPE_EMPTY: {
                    rowData.emplace_back(L"");
                    break;
                }
                default: {
                    auto t = sheet->readStr(row, col);
                    rowData.emplace_back(t);
                    break;
                }
            }
        }
    }
    return true;
}
#endif
