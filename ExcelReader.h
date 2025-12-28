#pragma once
#ifndef NO_XLS
#include "libxl.h"
#include <vector>
#include <string>
#include <chrono>
#include <iostream>
#include <boost/locale.hpp>

class ExcelReader {
    bool ReadSheetData(std::vector<std::vector<std::wstring>> &data);

public:
    libxl::Book *book;
    libxl::Sheet *sheet;
    unsigned int twoDigitFormat;
    libxl::Format *fmtNum;
    libxl::Format *fmtNumDec;
    libxl::Format *fmtCenter;
    libxl::Format *fmtRight;
    libxl::Format *fmt;
    libxl::Font *font;

    libxl::Font *fontHalf;

public:
    ExcelReader(bool isXLSX);
    virtual ~ExcelReader();
    bool Open(const std::wstring &filename);
    bool ReadSheet(std::vector<std::vector<std::wstring>> &data, const std::wstring &sheetName = L"");
    bool ReadSheet(std::vector<std::vector<std::wstring>> &data, int sheetIndex);
    std::wstring ReadCell(libxl::Sheet *sheet, int row, int col);
    std::wstring ReadCellDate(libxl::Sheet *sheet, int row, int col);
    std::wstring UnpackDate(double x);
    std::chrono::system_clock::time_point UnpackDateToChrono(double x);

    void WriteCell(libxl::Sheet *sheet, int row, int col, const std::wstring &v, bool isNumeric, bool isRightJustified) const ;

    static libxl::Book *CreateBook(bool isXLSX);
};
#endif
