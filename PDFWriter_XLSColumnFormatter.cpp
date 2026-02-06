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

#include "wx/file.h"
#include "wx/filename.h"
#include "wx/dir.h"
#include "wx/numformatter.h"
#include "wx/rawbmp.h"
#include "wx/tokenzr.h"
#include <iostream>
#include <fstream>
#include <memory>

#include "global.h"
#include "words.h"
#include "rDb.h"
#include "ExcelReader.h"
#include "PDFWriter.h"
#include "xmlParser.h"
#include "logger/logger.h"
#include <fmt/format.h>
#include <fmt/xchar.h>

double DB::XLSColumnFormatter::GetDouble(int i) {
    wxASSERT(i >= 0 && i < int(defArray.size()));
    ColumnDefinition &cdef = *defArray[i];
    if (cdef.expression) {
        DB::SetVector(cdef.expression.get(), data);
        return DB::ExecuteExpression(cdef.expression.get());
    }
    return rs->Get<double>(i);
}

wxLongLong DB::XLSColumnFormatter::GetValue(int i) {
    wxASSERT(i >= 0 && i < int(defArray.size()));
    ColumnDefinition &cdef = *defArray[i];
    if (cdef.expression) {
        DB::SetVector(cdef.expression.get(), data);
        return DB::ExecuteExpression(cdef.expression.get());
    }
    return rs->Get<double>(i);
}

std::wstring DB::XLSColumnFormatter::GetString(int i, bool toFormatNumber) {
    wxASSERT(i >= 0 && i < int(defArray.size()));
    ColumnDefinition &cdef = *defArray[i];
    if (!cdef.formula.empty()) return cdef.formula;
    if (rs->IsEOF()) return L"";
    std::wstring v = rs->Get<std::wstring>(i);
    bool isNumericString = String::IsNumeric(v);

    if (cdef.expression) {
        DB::SetVector(cdef.expression.get(), data);
        if (toFormatNumber)
            return wxNumberFormatter::ToString(DB::ExecuteExpression(cdef.expression.get()), cdef.precision).ToStdWstring();
        else
            return fmt::format(L"{:.{}f}", DB::ExecuteExpression(cdef.expression.get()), cdef.precision);
    }
    switch (cdef.type) {
        case Number:
            if (toFormatNumber) {
                if (cdef.precision > 0 || cdef.isDivideFactor) {
                    auto d = isNumericString ? rs->Get<double>(i) : 0.0;
                    v = wxNumberFormatter::ToString(d, cdef.precision, wxNumberFormatter::Style_WithThousandsSep);
                } else {
                    v = wxNumberFormatter::ToString(isNumericString ? rs->Get<wxLongLong>(i).ToLong() : 0, wxNumberFormatter::Style_WithThousandsSep);
                }
            }
            break;
        case Month: v = String::to_wstring(DB::SQLiteBase::GetMonthName(rs->Get<int>(i))); break;
        case WeekDay: v = String::to_wstring(DB::SQLiteBase::GetDayName(rs->Get<int>(i))); break;
        case Date: {
            wxDateTime t = rs->Get<wxDateTime>(i);
            v = ((t.IsValid()) ? t.Format("%d-%m-%Y") : wxString());
            break;
        }  //(wpDateFormat")
        case DateTime: {
            wxDateTime t = rs->Get<wxDateTime>(i);
            v = ((t.IsValid()) ? t.Format("%d-%m-%Y ~ %H:%M:%S") : wxString());
            break;
        }
        case Time: {
            wxDateTime t = rs->Get<wxDateTime>(i);
            v = ((t.IsValid()) ? t.Format("%H:%M:%S") : wxString());
            break;
        }
        case String: break;
    }
    return v;
}

void DB::XLSColumnFormatter::WriteString(long row, int col, int i, bool isHighlight) {
#ifndef NO_XLS
    wxASSERT(i >= 0 && i < int(defArray.size()));
    ColumnDefinition &cdef = *defArray[i];
    if (!cdef.formula.empty()) {
        auto actual = cdef.formula;
        int r = row + 1;
        boost::ireplace_all(actual, "@ROW", String::IntToString(r));
        boost::ireplace_all(actual, "@PREV-ROW", String::IntToString(r - 1));
        boost::ireplace_all(actual, "@NEXT-ROW", String::IntToString(r + 1));
        sheet->writeFormula(row, col, actual.c_str(), GetXLSFormat(col, isHighlight));
    } else {
        auto v = GetString(i);
        if (v.empty()) return;
        if (cdef.type == Number)
            sheet->writeNum(row, col, String::ToDouble(v), isHighlight ? cdef.fmtHighlight : cdef.fmt);
        else {
            sheet->writeStr(row, col, v.c_str(), isHighlight ? cdef.fmtHighlight : cdef.fmt);
        }
    }
#endif
}

wxJSONValue DB::XLSColumnFormatter::emptyJSON;

DB::XLSColumnFormatter::ColumnDefinition::ColumnDefinition() : fmt(NULL),
                                                               fmtHighlight(NULL),
                                                               subTotalLabel(false),
                                                               type(String),
                                                               length(-1),
                                                               precision(0),
                                                               isDivideFactor(false),
                                                               size(0),
                                                               pageTotal(0),
                                                               grandTotal(0),
                                                               nRecTotal(0),
                                                               nRecPage(0) {}

#ifndef NO_XLS
DB::XLSColumnFormatter::XLSColumnFormatter(ExcelReader *xlReader, libxl::Sheet *xlsSheet, std::shared_ptr<wpSQLResultSet> resultSet, bool freezeHeader, wxJSONValue &param) : rs(resultSet), xlr(xlReader), sheet(xlsSheet) {
#else
DB::XLSColumnFormatter::XLSColumnFormatter(void *, void *, std::shared_ptr<wpSQLResultSet> resultSet, bool freezeHeader, wxJSONValue &param) : rs(resultSet) {
#endif
    if (!param.HasMember("numberOfdimensions")) param["numberOfdimensions"] = 1;
    unsigned int nDim = wxAtol(param["numberOfdimensions"].AsString());
    // int nMeasurements = param["aggregateFunction"].Size();

    int row = 0, col = 0;
#ifndef NO_XLS
    row = (sheet ? sheet->lastRow() : 0) + 1;
#else
    row = 1;
#endif
    std::wstring actualColumnName;
    constexpr int BUF_LEN = 2048;
    const unsigned int nCol = rs->GetColumnCount();
    data = nullptr;
    bool hasExpression = false;
    bool defaultNumberFormatter = param.HasMember("default-number-dividefactor") ? param["default-number-dividefactor"].AsBool() : false;
    for (unsigned int i = 0; i < nCol; i++) {
        auto &colDef = defArray.emplace_back(new ColumnDefinition());
        ColumnDefinition &cdef = *colDef;

        cdef.colName = String::to_wstring(rs->GetColumnName(i));
        if (param.HasMember("subtotal")) {  // from @footer
            wxJSONValue &v = param["subtotal"];
            if (v.IsArray() && v.Size() > i)
                cdef.sumFunction = v[i].AsString();
        }

        if (boost::iequals(cdef.sumFunction, "sum") || boost::iequals(cdef.sumFunction, "avg")) {
            ;
        } else if (boost::equals(cdef.sumFunction, L"*"))
            cdef.subTotalLabel = true;
        else if (!cdef.sumFunction.empty()) {
            cdef.footerExpression = DB::CreateExpression(cdef.sumFunction, nCol);
        }

        if (param.HasMember("row-function")) {
            wxJSONValue &v = param["row-function"];
            if (v.IsArray() && v.Size() > i) {
                std::wstring expr(v[i].AsString());
                if (!expr.empty()) {
                    // if (cdef.expression) DB::DestroyExpression(cdef.expression);
                    cdef.expression = DB::CreateExpression(expr, nCol);
                    hasExpression = true;
                }
            }
        }
        if (param.HasMember("column-sizes")) {
            wxJSONValue &v = param["column-sizes"];
            if (v.IsArray() && v.Size() > i) {
                cdef.size = v[i].AsInt();
            }
        }
        size_t x;
        wchar_t f[BUF_LEN];
        memset(f, 0, BUF_LEN * sizeof(wchar_t));
        if ((x = cdef.colName.find(L"@sum")) != std::wstring::npos) {  // @sum=avg or @sum=sum
            auto s = cdef.colName.substr(x + 4);
            auto *optr = s.c_str();
            auto *resto = String::SkipWhiteSpace(optr);
            if (*resto == '=') {
                resto = String::SkipWhiteSpace(++resto);
                resto = String::CopyUntilSpace(f, resto, BUF_LEN);
                cdef.sumFunction = f;
                int l = resto - optr;
                auto toRep = cdef.colName.substr(x, l + 4);
                boost::replace_all(cdef.colName, toRep, L"");
            }
        }
        if ((x = cdef.colName.find(L"@formula")) != std::wstring::npos) {  // @sum=avg or @sum=sum
            auto s = cdef.colName.substr(x + 8);
            auto *optr = s.c_str();
            auto *resto = String::SkipWhiteSpace(optr);
            if (*resto == '=') {
                resto = String::SkipWhiteSpace(++resto);
                resto = String::CopyUntilSpace(f, resto, BUF_LEN);
                cdef.formula = boost::to_upper_copy(std::wstring(f));
                int l = resto - optr;
                auto toRep = cdef.colName.substr(x, l + 8);
                boost::replace_all(cdef.colName, toRep, "");
            }
        }
        if (defaultNumberFormatter && i > 0) {  // first one not converted to string.
            cdef.isDivideFactor = true;
            cdef.type = Number;
            boost::replace_all(cdef.colName, L"@dividefactor", L"");
#ifndef NO_XLS
            if (xlr) cdef.fmt = xlr->fmtNumDec;
#endif
            cdef.precision = 2;
        }

        if ((x = cdef.colName.find(L"@function")) != std::wstring::npos) {  // @sum=avg or @sum=sum
            auto s = cdef.colName.substr(x + 9);
            auto *optr = s.c_str();
            auto *resto = String::SkipWhiteSpace(optr);
            if (*resto == '=') {
                resto = String::SkipWhiteSpace(++resto);
                resto = String::CopyUntilSpace(f, resto, sizeof(f));
                if (!String::IsEmpty(f, sizeof(f))) {
                    cdef.expression = DB::CreateExpression(std::wstring(f, std::wcslen(f)), nCol);
                    hasExpression = true;
                }
                auto l = resto - optr;
                auto toRep = cdef.colName.substr(x, l + 9);
                boost::replace_all(cdef.colName, toRep, L"");
            }
        } else if (boost::icontains(cdef.colName, L"@dividefactor")) {
            cdef.isDivideFactor = true;
            cdef.type = Number;
            boost::replace_all(cdef.colName, L"@dividefactor", L"");
#ifndef NO_XLS
            if (xlr) cdef.fmt = xlr->fmtNumDec;
#endif
            cdef.precision = 2;
        }
        if (boost::icontains(cdef.colName, L"@center")) {
            boost::ireplace_all(cdef.colName, L"@center", L"");
#ifndef NO_XLS
            if (xlr) cdef.fmt = xlr->fmtCenter;
#endif
        } else if (boost::icontains(cdef.colName, L"@integer")) {
            boost::ireplace_all(cdef.colName, L"@integer", L"");
#ifndef NO_XLS
            if (xlr) cdef.fmt = xlr->fmtNum;
#endif
            cdef.type = Number;
            cdef.precision = 0;
        } else if (boost::icontains(cdef.colName, L"@number")) {
            boost::ireplace_all(cdef.colName, L"@number", L"");
#ifndef NO_XLS
            if (xlr) cdef.fmt = xlr->fmtNum;
#endif
            cdef.type = Number;
            cdef.precision = 0;
        } else if (boost::icontains(cdef.colName, L"@decimal")) {
            boost::ireplace_all(cdef.colName, L"@decimal", L"");
#ifndef NO_XLS
            if (xlr) cdef.fmt = xlr->fmtNumDec;
#endif
            cdef.type = Number;
            cdef.precision = 2;
        } else if (boost::icontains(cdef.colName, L"@decimal4")) {
            boost::ireplace_all(cdef.colName, L"@decimal4", L"");
#ifndef NO_XLS
            if (xlr) cdef.fmt = xlr->fmtNumDec;
#endif
            cdef.type = Number;
            cdef.precision = 4;
        } else if (boost::icontains(cdef.colName, L"@month")) {
            boost::ireplace_all(cdef.colName, L"@month", L"");
            cdef.type = Month;
        } else if (boost::icontains(cdef.colName, L"@weekday")) {
            boost::ireplace_all(cdef.colName, L"@weekday", L"");
            cdef.type = WeekDay;
        } else if (boost::icontains(cdef.colName, L"@datetime")) {
            boost::ireplace_all(cdef.colName, L"@datetime", L"");
            cdef.type = DateTime;
        } else if (boost::icontains(cdef.colName, L"@date")) {
            boost::ireplace_all(cdef.colName, L"@date", L"");
            cdef.type = Date;
        } else if (boost::icontains(cdef.colName, L"@time")) {
            boost::ireplace_all(cdef.colName, L"@time", L"");
            cdef.type = Time;
        }
#ifndef NO_XLS
        if (sheet && xlr) {
            if (i < nDim) {
                actualColumnName.append((i == 0 ? L"" : L"/") + cdef.colName);
                if (sheet) sheet->writeStr(row, col, actualColumnName.c_str(), xlr->fmt);
            } else {
                if (i == nDim && nDim > 0) col = 1;
                if (sheet) sheet->writeStr(row, col++, cdef.colName.c_str(), xlr->fmt);
            }
            if (freezeHeader) sheet->split(row + 1, 0);  // freeze headers
            cdef.fmtHighlight = xlr->book->addFormat(cdef.fmt);
            cdef.fmtHighlight->setFillPattern(libxl::FILLPATTERN_SOLID);
            cdef.fmtHighlight->setPatternForegroundColor(libxl::COLOR_LIGHTGREEN);
        }
#endif
    }
    if (hasExpression) {
        data = new double[nCol];
        memset(data, 0, sizeof(double) * nCol);
    }
}

DB::XLSColumnFormatter::~XLSColumnFormatter() {
    if (data) delete[] data;
    for (auto const &it : defArray) {
        delete it;
    }
    defArray.clear();
}
