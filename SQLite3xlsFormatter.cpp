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
//#include <stdio.h>
#include <iostream>
#include <fstream>
#include <memory>

#include "global.h"
#include "words.h"
#include "rDb.h"
#include "ExcelReader.h"
#include "PDFWriter.h"
#include "html_report_builder.h"
#include "xmlParser.h"
#include "logger/logger.h"
#include "SQLite3xlsFormatter.h"

bool g_useXLSXformat = false;

namespace ReportGenerator {

Generator::Function Generator::GetProcessor(const std::string &cmdLine, wxJSONValue &param) {
    //	@group(nDim, aggregate0, aggregate1, ...)
    Function fn = nullptr;
    if (param.HasMember("output-type") && param["output-type"].AsString().IsSameAs("xls", false))
        fn = &Generator::AppendToExcelSheet;
    else
        fn = &Generator::AppendToPDF;
    const char *s = cmdLine.c_str();
    char word[1024];
    s = String::CopyUntilChar(word, s, '(', sizeof(word));
    std::string fname = boost::trim_copy(std::string(word));
    std::string exprString = boost::trim_copy(std::string(s));
    if (!exprString.empty() && exprString.back() == ')') exprString.pop_back();
    param["default-number-dividefactor"] = false;

    if (boost::iequals(fname, "@group")) {  // @group(nDim, agg, agg, agg, ...);
        if (param.HasMember("output-type") && param["output-type"].AsString().IsSameAs("xls", false))
            fn = &AppendGroupingToExcelSheet;
        wxStringTokenizer t2(exprString, ",", wxTOKEN_RET_EMPTY_ALL);
        if (t2.HasMoreTokens()) {
            std::string v(t2.GetNextToken());
            param["numberOfdimensions"] = v;
        }
        for (int i = 0; t2.HasMoreTokens(); i++) {
            std::string v(t2.GetNextToken());
            param["aggregateFunction"][i] = v;
        }
    } else if (boost::iequals(fname, "@removedup")) {  // @removedup(col, col, col, ...) - check if same as previous, blankit
        if (param.HasMember("output-type") && param["output-type"].AsString().IsSameAs("xls", false))
            fn = &Generator::AppendToExcelSheet;
        else
            fn = &Generator::AppendToPDF;
        wxStringTokenizer t2(exprString, ",");
        for (int i = 0; t2.HasMoreTokens(); i++) {
            std::string v(t2.GetNextToken());
            param["removedup"][i] = wxAtol(v);
        }
    } else if (boost::iequals(fname, "@default-number-dividefactor")) {  // @key-columns(col, col, col, ...)
        param["default-number-dividefactor"] = true;
    } else if (boost::iequals(fname, "@key-columns")) {  // @key-columns(col, col, col, ...)
        if (param.HasMember("output-type") && param["output-type"].AsString().IsSameAs("xls", false))
            fn = &Generator::AppendToExcelSheet;
        else
            fn = &Generator::AppendToPDF;
        wxStringTokenizer t2(exprString, ",");
        for (int i = 0; t2.HasMoreTokens(); i++) {
            std::string v(t2.GetNextToken());
            param["key-columns"][i] = wxAtol(v);
        }
    } else if (boost::iequals(fname, "@show-all-keys")) {  // @show-all-keys(col, col, col, ...)
        if (param.HasMember("output-type") && param["output-type"].AsString().IsSameAs("xls", false))
            fn = &Generator::AppendToExcelSheet;
        else
            fn = &Generator::AppendToPDF;
        param["show-all-keys"] = true;
    } else if (boost::iequals(fname, "@break-page")) {  // @break-page(col, col, col, ...)
        if (param.HasMember("output-type") && param["output-type"].AsString().IsSameAs("xls", false))
            fn = &Generator::AppendToExcelSheet;
        else
            fn = &Generator::AppendToPDF;
        wxStringTokenizer t2(exprString, ",");
        for (int i = 0; t2.HasMoreTokens(); i++) {
            std::string v(t2.GetNextToken());
            param["break-page"][i] = wxAtol(v);
        }
    } else if (boost::iequals(fname, "@lineheight")) {  // @lineheight(nDim, agg, agg, agg, ...);
        wxStringTokenizer t2(exprString, ",", wxTOKEN_RET_EMPTY_ALL);
        if (t2.HasMoreTokens()) {
            std::string v (t2.GetNextToken());
            param["lineheight"] = wxAtol(v);
        }
    } else if (boost::iequals(fname, "@font")) {  // @font(nDim, agg, agg, agg, ...);
        wxStringTokenizer t2(exprString, ",", wxTOKEN_RET_EMPTY_ALL);
        if (t2.HasMoreTokens()) {
            std::string v (t2.GetNextToken());
            param["fontname"] = v;
        }
        if (t2.HasMoreTokens()) {
            std::string v (t2.GetNextToken());
            param["fonttype"] = v;
        }
        if (t2.HasMoreTokens()) {
            std::string v (t2.GetNextToken());
            param["fontsize"] = wxAtol(v);
        }
    } else if (boost::iequals(fname, "@footer")) {  // @footer(agg, agg, agg, agg, ...);
        wxStringTokenizer t2(exprString, "|", wxTOKEN_RET_EMPTY_ALL);
        wxJSONValue array;
        while (t2.HasMoreTokens()) {
            std::string v (t2.GetNextToken());
            array.Append(v);
        }
        param["subtotal"] = array;
    } else if (boost::iequals(fname, "@row-function")) {  // @footer(agg, agg, agg, agg, ...);
        wxStringTokenizer t2(exprString, "|", wxTOKEN_RET_EMPTY_ALL);
        wxJSONValue array;
        while (t2.HasMoreTokens()) {
            std::string v (t2.GetNextToken());
            array.Append(v);
        }
        param["row-function"] = array;
    } else if (boost::iequals(fname, "@column-sizes")) {  // @footer(agg, agg, agg, agg, ...);
        wxStringTokenizer t2(exprString, "|", wxTOKEN_RET_EMPTY_ALL);
        wxJSONValue array;
        while (t2.HasMoreTokens()) {
            int v = wxAtol(t2.GetNextToken());
            array.Append(v);
        }
        param["column-sizes"] = array;
    } else if (boost::iequals(fname, "@clear")) {  // @footer(agg, agg, agg, agg, ...);
        param.Clear();
    }
    return fn;
}

/*
@footer(|*|sum|sum|sum|sum|sum|c[6]/c[3]*100)
select
        'DAILY SALES' as 'REPORT',
        type as 'TYPE',
        gst as "GST@dividefactor",
        amount as "VALUE@dividefactor",
        collection as "COLLECTION@dividefactor",
        cost as "COST@dividefactor",
        gross as "GROSS@dividefactor",
        0 as "MARGIN@decimal@function=c[6]/c[3]*100"
from ...
*/

static bool DimValueChanges(std::vector<std::wstring> &dimValues, std::vector<std::wstring> &prevDimValues, int nToCheck) {
    for (int i = 0; i < nToCheck; i++) {
        if (!boost::equals(dimValues[i], prevDimValues[i])) return false;
    }
    return true;
}

#ifndef NO_XLS
std::string ReportGenerator::Generator::WriteToExcel(std::vector<std::pair<std::shared_ptr<wpSQLResultSet>, std::vector<int>>> &rsList, const std::string &title, const std::string &subTitle, const std::string &tabName) {
    std::vector<std::pair<std::shared_ptr<wpSQLResultSet>, std::vector<int>>>::iterator it = rsList.begin();
    if (it == rsList.end()) return "";

    std::shared_ptr<wpSQLResultSet> &rs = it->first;
    const int nCol = rs->GetColumnCount();

    ExcelReader xlr(g_useXLSXformat);
    unsigned int twoDigitFormat = xlr.book->addCustomNumFormat(L"#0.00");
    libxl::Format *fmtNum = xlr.book->addFormat();
    fmtNum->setNumFormat(libxl::NUMFORMAT_NUMBER_SEP);
    fmtNum->setAlignH(libxl::ALIGNH_RIGHT);
    fmtNum->setAlignV(libxl::ALIGNV_CENTER);
    libxl::Format *fmtNumDec = xlr.book->addFormat();
    fmtNumDec->setNumFormat(twoDigitFormat);
    fmtNumDec->setAlignH(libxl::ALIGNH_RIGHT);
    fmtNumDec->setAlignV(libxl::ALIGNV_CENTER);
    libxl::Format *fmtCenter = xlr.book->addFormat();
    fmtCenter->setAlignH(libxl::ALIGNH_CENTER);
    fmtCenter->setAlignV(libxl::ALIGNV_CENTER);
    libxl::Format *fmtRight = xlr.book->addFormat();
    fmtRight->setAlignH(libxl::ALIGNH_RIGHT);
    fmtRight->setAlignV(libxl::ALIGNV_CENTER);
    // libxl::Format *fmt = NULL;

    libxl::Sheet *sheet = xlr.book->addSheet(String::to_wstring(tabName).c_str());

    std::string fileName = String::CreateTempFileName("kdr_");

    int row = 0, col = 0;

    libxl::Font *font = xlr.book->addFont();
    font->setSize(20);
    libxl::Format *fmtTitle = xlr.book->addFormat();
    fmtTitle->setAlignH(libxl::ALIGNH_CENTER);
    fmtTitle->setAlignV(libxl::ALIGNV_CENTER);
    fmtTitle->setFont(font);

    libxl::Font *fontHalf = xlr.book->addFont();
    fontHalf->setSize(15);
    libxl::Format *fmtSubTitle = xlr.book->addFormat();
    fmtSubTitle->setAlignH(libxl::ALIGNH_LEFT);
    fmtSubTitle->setAlignV(libxl::ALIGNV_CENTER);
    fmtSubTitle->setFont(fontHalf);
    libxl::Format *fmtSubTitleRight = xlr.book->addFormat();
    fmtSubTitleRight->setAlignH(libxl::ALIGNH_RIGHT);
    fmtSubTitleRight->setAlignV(libxl::ALIGNV_CENTER);
    fmtSubTitleRight->setFont(fontHalf);

    sheet->writeStr(row, 0, String::to_wstring(title).c_str(), fmtTitle);
    sheet->setRow(row, 30);
    sheet->setMerge(row, row, 0, nCol - 1);
    row++;

    sheet->writeStr(row, 0, String::to_wstring(subTitle).c_str(), fmtSubTitle);
    sheet->writeStr(row, nCol - 1, String::to_wstring(wxDateTime::Now().Format()).c_str(), fmtSubTitleRight);
    sheet->setRow(row, 15);

    for (auto const &innerIt : rsList) {
        std::shared_ptr<wpSQLResultSet> rsLocal = innerIt.first;
        const int numberOfCols = rsLocal->GetColumnCount();
        DB::XLSColumnFormatter formatter(&xlr, sheet, rsLocal);
        row = sheet->lastRow() + 1;
        while (rsLocal->NextRow()) {
            col = 0;
            for (int i = 0; i < numberOfCols; i++, col++) {
                formatter.WriteString(row, col, i);
            }
            row++;
        }
    }
    xlr.book->save(String::to_wstring(fileName).c_str());
    return fileName;
}
#endif

#ifndef NO_XLS
libxl::Sheet *ReportGenerator::Generator::CreateNewSheet(ExcelReader *xlsReader, int nCols, const std::string &sheetName, const std::string &title, const std::string &subTitle) {
    ExcelReader &xlr(*xlsReader);
    libxl::Sheet *sheet = xlr.book->addSheet(String::to_wstring(sheetName).c_str());

    int row = 0;

    if (!title.empty()) {
        libxl::Format *fmtTitle = xlr.book->addFormat();
        fmtTitle->setAlignH(libxl::ALIGNH_CENTER);
        fmtTitle->setAlignV(libxl::ALIGNV_CENTER);
        fmtTitle->setFont(xlr.font);
        sheet->writeStr(row, 0, String::to_wstring(title).c_str(), fmtTitle);
        sheet->setRow(row, 30);
        sheet->setMerge(row, row, 0, nCols - 1);
        row++;
    }
    if (!subTitle.empty()) {
        libxl::Format *fmtSubTitle = xlr.book->addFormat();
        fmtSubTitle->setAlignH(libxl::ALIGNH_LEFT);
        fmtSubTitle->setAlignV(libxl::ALIGNV_CENTER);
        fmtSubTitle->setFont(xlr.fontHalf);
        sheet->writeStr(row, 0, String::to_wstring(subTitle).c_str(), fmtSubTitle);
        sheet->writeStr(row, nCols - 1, String::to_wstring(wxDateTime::Now().Format("%d-%m-%Y %a %X")).c_str(), fmtSubTitle);
        sheet->setRow(row, 15);
    }
    return sheet;
}
#endif

#ifndef NO_XLS
libxl::Sheet *ReportGenerator::Generator::AppendGroupingToExcelSheet(ExcelReader *xlsReader, libxl::Sheet *sheet, std::shared_ptr<HtmlReportBuilder> /*unused*/, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader) {
    if (!xlsReader || !sheet) return NULL;
    ExcelReader &xlr(*xlsReader);
    int row = 0, col = 0;
    DB::XLSColumnFormatter formatter(&xlr, sheet, rs, freezeHeader, param);
    row = sheet->lastRow() + 1;
    const int nCol = rs->GetColumnCount();
    std::vector<int> colsize;
    colsize.resize(nCol);
    int startingRow = row;

    std::vector<std::wstring> dimValues;
    if (!param.HasMember("numberOfdimensions")) param["numberOfdimensions"] = 1;
    int nDim = wxAtol(param["numberOfdimensions"].AsString());
    dimValues.resize(nDim);

    wxJSONValue aggFunctions = param["aggregateFunction"];
    int nMeasurements = aggFunctions.Size();

    std::vector<std::wstring> prevDimValues = dimValues;
    std::vector<std::wstring> globalTotalFormula;
    for (int i = 0; i < nMeasurements; i++) {
        globalTotalFormula.emplace_back(std::wstring(aggFunctions[i].AsString()) + L"(");
    }
    bool first = true;
    std::wstring fdelim;
    while (rs->NextRow()) {
        for (int i = 0; i < nDim; i++) {
            dimValues[i] = formatter.GetString(i);
        }
        if (DimValueChanges(dimValues, prevDimValues, nDim - 1)) {
            sheet->writeStr(row, 0, (std::wstring(L"   ") + dimValues[1]).c_str(), formatter.GetXLSFormat(1));
        } else {
            if (!first) {
                // bool groupRows(int rowFirst, int rowLast, bool collapsed = true);
                sheet->groupRows(startingRow + 1, row - 1, true);
                for (int i = 0; i < nMeasurements; i++) {
                    std::string aggFunc(aggFunctions[i].AsString().Trim().Trim(false));
                    if (!aggFunc.empty()) {
                        std::wstring firstRow = sheet->rowColToAddr(startingRow + 1, i + 1);
                        std::wstring lastRow = sheet->rowColToAddr(row - 1, i + 1);
                        auto formula = (aggFunctions[i].AsString() + "(" + firstRow + ":" + lastRow + ")");
                        sheet->writeFormula(startingRow, i + 1, formula.wc_str(), formatter.GetXLSFormat(i + nDim, true));
                        auto v = fdelim + std::wstring(sheet->rowColToAddr(startingRow, i + 1));
                        globalTotalFormula[i].append(v);
                    } else if (formatter.IsFormula(i + nDim))
                        formatter.WriteString(startingRow, i + 1, i + nDim, true);
                    else
                        sheet->writeStr(startingRow, i + 1, L"", formatter.GetXLSFormat(i + nDim, true));
                }
                fdelim = L",";
            }
            startingRow = row;
            sheet->writeStr(row++, 0, dimValues[0].c_str(), formatter.GetXLSFormat(0, true));
            sheet->writeStr(row, 0, (std::wstring(L"   ") + dimValues[1]).c_str(), formatter.GetXLSFormat(1));
            first = false;
            prevDimValues = dimValues;
        }
        col = 1;
        for (int i = nDim; i < nCol; i++) {
            formatter.WriteString(row, col++, i);
        }
        row++;
    }
    sheet->groupRows(startingRow + 1, row - 1, true);
    for (int i = 0; i < nMeasurements; i++) {
        if (!aggFunctions[i].AsString().empty()) {
            std::wstring firstRow = sheet->rowColToAddr(startingRow + 1, i + 1);
            std::wstring lastRow = sheet->rowColToAddr(row - 1, i + 1);
            auto formula = aggFunctions[i].AsString() + "(" + firstRow + ":" + lastRow + ")";
            sheet->writeFormula(startingRow, i + 1, formula.wc_str(), formatter.GetXLSFormat(i + nDim, true));
            globalTotalFormula[i].append(fdelim + std::wstring(sheet->rowColToAddr(startingRow, i + 1)) + L")");
        } else if (formatter.IsFormula(i + nDim))
            formatter.WriteString(startingRow, i + 1, i + nDim, true);
        else
            sheet->writeStr(startingRow, i + 1, L"", formatter.GetXLSFormat(i + nDim, true));
    }

    sheet->writeStr(row, 0, L"*** TOTAL ***", formatter.GetXLSFormat(1, true));
    for (int i = 0; i < nMeasurements; i++) {
        if (!aggFunctions[i].AsString().empty()) {
            auto formula = globalTotalFormula[i];
            sheet->writeFormula(row, i + 1, formula.c_str(), formatter.GetXLSFormat(i + nDim, true));
        } else if (formatter.IsFormula(i + nDim))
            formatter.WriteString(row, i + 1, i + nDim, true);
        else
            sheet->writeStr(row, i + 1, L"", formatter.GetXLSFormat(i + nDim, true));
    }
    for (int i = 0; i < col; i++) {
        sheet->setCol(i, i, formatter.GetColumnSize(i));
    }
    return sheet;
}

libxl::Sheet *ReportGenerator::Generator::AppendToExcelSheet(ExcelReader *xlsReader, libxl::Sheet *sheet, std::shared_ptr<HtmlReportBuilder> /*unused*/, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader) {
    if (!xlsReader || !sheet) return NULL;
    ExcelReader &xlr(*xlsReader);
    int row = 0, col = 0;
    DB::XLSColumnFormatter formatter(&xlr, sheet, rs, freezeHeader);
    row = sheet->lastRow();

    std::vector<std::wstring> dimValues;
    const int nCol = rs->GetColumnCount();
    dimValues.resize(nCol);
    std::vector<bool> toCheckDup;
    toCheckDup.resize(nCol);
    if (param.HasMember("removedup")) {
        for (unsigned int i = 0; i < param["removedup"].Size(); i++) {
            if (param.HasMember("removedup"))
                toCheckDup[param["removedup"][i].AsInt()] = true;
        }
    }
    int startingRow = row;
    for (; rs->NextRow(); row++) {
        col = 0;
        for (int i = 0; i < nCol; i++, col++) {
            if (formatter.IsFormula(i)) {
                formatter.WriteString(row, col, i);
                continue;
            }
            auto v = formatter.GetString(i);
            bool toShow = true;
            if (toCheckDup[i]) {
                if (boost::equals(v, dimValues[i])) toShow = false;
                dimValues[i] = v;
            }
            if (toShow) {
                if (formatter.IsNumber(i) || String::IsNumeric(v))
                    sheet->writeNum(row, col, String::ToDouble(v), formatter.GetXLSFormat(i));
                else
                    sheet->writeStr(row, col, v.c_str(), formatter.GetXLSFormat(i));
            }
        }
    }
    for (int c = 0; c < col; c++) {
        sheet->setCol(c, c, formatter.GetColumnSize(c));
        auto sumFunction = boost::to_upper_copy(formatter.GetSumFunction(c));
        if (sumFunction.empty()) {
            formatter.WriteString(row, c, c);
            continue;
        }
        std::wstring firstRow = sheet->rowColToAddr(startingRow, c);
        std::wstring lastRow = sheet->rowColToAddr(row - 1, c);
        std::wstring formula = sumFunction + L"(" + firstRow + L":" + lastRow + L")";
        if (!sheet->writeFormula(row, c, formula.c_str(), formatter.GetXLSFormat(c))) {
            std::string x = String::to_string(formula);
            LOG_ERROR("libxl.version={}/{}> Error {}: [{}]", xlr.book->version(), xlr.book->biffVersion(), xlr.book->errorMessage(), x);
        }
    }
    return sheet;
}
#endif

std::shared_ptr<HtmlReportBuilder> ReportGenerator::Generator::CreateNewPDF(std::shared_ptr<wpSQLResultSet> rs, const std::wstring &orientation, const std::wstring &sectionName, const std::wstring &title, const std::wstring &subTitle, wxJSONValue &param, const std::wstring outletName) {
    LOG_INFO("CreateNewPDF (HtmlReportBuilder)");
    std::string orient = "Portrait";
    if (boost::iequals(orientation, "Landscape") || boost::iequals(orientation, "L")) orient = "Landscape";
    auto builder = std::make_shared<HtmlReportBuilder>(String::to_string(title), String::to_string(outletName), orient);
    builder->setSubtitle(String::to_string(subTitle));
    if (rs == nullptr) return builder;

    const int nCol = rs->GetColumnCount();
    // Create XLSColumnFormatter for data formatting
    auto *c = new DB::XLSColumnFormatter(NULL, NULL, rs, false, param);

    // Transfer column definitions to builder
    for (int i = 0; i < nCol; i++) {
        auto &cdef = *c->def()[i];
        builder->addColumn(
            String::to_string(cdef.colName),
            cdef.size > 0 ? cdef.size : 1.0,
            cdef.type == DB::XLSColumnFormatter::Number,
            String::to_string(cdef.sumFunction));
    }

    builder->setBreakPageOn(param.HasMember("break-page"));

    // Store formatter pointer for later use in AppendToPDF
    // (formatter is managed via the builder's user data — we store it and delete in AppendToPDF)
    builder->setFormatterPtr(c);

    return builder;
}

static bool IsSameKey(std::vector<bool> &keyColumns, std::vector<std::string> &prevValues, std::vector<std::string> &currValues) {
    wxASSERT(keyColumns.size() == prevValues.size());
    for (int i = 0; i < int(keyColumns.size()); i++) {
        if (keyColumns[i]) {
            if (prevValues[i] != currValues[i])
                return false;
        }
    }
    return true;
}

// Helper: compute page total from formatter and set on builder's current section
static void computeAndSetPageTotal(HtmlReportBuilder &builder, DB::XLSColumnFormatter *fmt, int nCol, int startOfs) {
    bool hasSubtotal = false;
    for (int i = 0; i < nCol; i++) {
        if (!fmt->def()[i]->sumFunction.empty()) { hasSubtotal = true; break; }
    }
    if (!hasSubtotal) return;

    std::vector<std::string> totalCells(nCol);
    for (int i = startOfs; i < nCol; i++) {
        auto &cdef = *fmt->def()[i];
        if (cdef.subTotalLabel) {
            totalCells[i] = "TOTAL THIS PAGE";
        } else if (cdef.type == DB::XLSColumnFormatter::Number) {
            if (boost::iequals(cdef.sumFunction, "sum"))
                totalCells[i] = String::to_string(wxNumberFormatter::ToString(cdef.pageTotal, cdef.precision));
            else if (boost::iequals(cdef.sumFunction, "average") || boost::iequals(cdef.sumFunction, "avg"))
                totalCells[i] = String::to_string(wxNumberFormatter::ToString((cdef.nRecPage != 0 ? cdef.pageTotal / cdef.nRecPage : 0.0), cdef.precision));
        }
        // Reset page counters for next section
        cdef.pageTotal = 0;
        cdef.nRecPage = 0;
    }
    builder.setPageTotal(totalCells);
}

// Helper: compute grand total from formatter and set on builder
static void computeAndSetGrandTotal(HtmlReportBuilder &builder, DB::XLSColumnFormatter *fmt, int nCol, int startOfs) {
    bool hasSubtotal = false;
    for (int i = 0; i < nCol; i++) {
        if (!fmt->def()[i]->sumFunction.empty()) { hasSubtotal = true; break; }
    }
    if (!hasSubtotal) return;

    std::vector<std::string> totalCells(nCol);
    for (int i = startOfs; i < nCol; i++) {
        auto &cdef = *fmt->def()[i];
        if (cdef.subTotalLabel) {
            totalCells[i] = "GRAND TOTAL";
        } else if (cdef.type == DB::XLSColumnFormatter::Number) {
            if (boost::iequals(cdef.sumFunction, "sum"))
                totalCells[i] = String::to_string(wxNumberFormatter::ToString(cdef.grandTotal, cdef.precision));
            else if (boost::iequals(cdef.sumFunction, "average") || boost::iequals(cdef.sumFunction, "avg"))
                totalCells[i] = String::to_string(wxNumberFormatter::ToString((cdef.nRecTotal != 0 ? cdef.grandTotal / cdef.nRecTotal : 0), cdef.precision));
        }
    }
    builder.setGrandTotal(totalCells);
}

// #ifdef NO_XLS
libxl::Sheet *ReportGenerator::Generator::AppendToPDF(ExcelReader *, libxl::Sheet *, std::shared_ptr<HtmlReportBuilder> report, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool /*freezeHeader*/) {
// #endif

    if (!report) throw std::runtime_error("AppendToPDF: report is NULL!!!");

    HtmlReportBuilder &builder(*report);
    DB::XLSColumnFormatter *fmt = builder.formatterPtr<DB::XLSColumnFormatter>();
    if (!fmt) throw std::runtime_error("AppendToPDF: formatter is NULL!");

    const int nCol = rs->GetColumnCount();
    int startOfs = (builder.breakPageOn() ? 1 : 0);

    // Check for formula columns
    bool formulaExists = false;
    for (int i = 0; i < nCol; i++) {
        DB::XLSColumnFormatter::ColumnDefinition &cdef = *fmt->def()[i];
        if (cdef.expression) {
            formulaExists = true;
            break;
        }
    }

    // Key column deduplication setup
    std::vector<bool> keyColumns(nCol, false);
    bool compareKey = false;
    bool showAllKeys = param.HasMember("show-all-keys") && (param["show-all-keys"].AsBool());
    if (param.HasMember("key-columns")) {
        for (unsigned int i = 0; i < param["key-columns"].Size(); i++) {
            int j = param["key-columns"][i].AsInt();
            if (j >= 0) keyColumns[j] = true;
            compareKey = true;
        }
    }
    std::vector<std::string> prevValues(nCol);
    std::vector<std::string> currValues(nCol);
    bool isSameKey = false;

    // Current section's page title for break-page
    std::string currentPageTitle;
    bool firstRow = true;

    // Start first section
    builder.newSection();

    while (rs->NextRow()) {
        // Handle break-page: new section when column 0 value changes
        if (builder.breakPageOn()) {
            auto v = String::to_string(fmt->GetString(0, true));
            if (v != currentPageTitle) {
                if (!firstRow) {
                    // Compute page totals for the ending section
                    computeAndSetPageTotal(builder, fmt, nCol, startOfs);
                    builder.newSection(v);
                } else {
                    // Set title on first section
                    if (builder.currentSection())
                        builder.currentSection()->pageTitle = v;
                }
                currentPageTitle = v;
            }
        }

        // Formula evaluation
        if (formulaExists) {
            for (int i = 0; i < nCol; i++) {
                DB::XLSColumnFormatter::ColumnDefinition &cdef = *fmt->def()[i];
                if (cdef.type == DB::XLSColumnFormatter::Number && fmt->data)
                    fmt->data[i] = fmt->GetDouble(i);
            }
        }

        // Key comparison for grouping
        if (compareKey) {
            for (int i = 0; i < nCol; i++) {
                currValues[i] = String::to_string(fmt->GetString(i, true));
            }
            isSameKey = IsSameKey(keyColumns, prevValues, currValues);
            if (!isSameKey) {
                prevValues = currValues;
            }
        }

        // Build row cells
        std::vector<std::string> rowCells(nCol);
        for (int i = 0; i < nCol; i++) {
            auto &cdef = *fmt->def()[i];
            std::string v = String::to_string(fmt->GetString(i, true));

            // Track page and grand totals for numeric columns
            if (cdef.type == DB::XLSColumnFormatter::Number) {
                double t = fmt->GetDouble(i);
                cdef.pageTotal += t;
                cdef.grandTotal += t;
                cdef.nRecPage++;
                cdef.nRecTotal++;
            }

            // Handle key deduplication: suppress display if same key
            if (compareKey && keyColumns[i]) {
                bool valueChanges = (v != prevValues[i]);
                if (!valueChanges && !showAllKeys) {
                    rowCells[i] = "";
                    continue;
                }
            }

            // Replace newlines with <br> for HTML rendering
            if (boost::contains(v, "\n")) {
                boost::replace_all(v, "\n", "<br>");
            }
            rowCells[i] = v;
        }

        builder.addRow(rowCells);
        firstRow = false;
    }

    // Compute page total for the last section
    computeAndSetPageTotal(builder, fmt, nCol, startOfs);

    // Compute and set grand total
    computeAndSetGrandTotal(builder, fmt, nCol, startOfs);

    return nullptr;
}

#ifndef NO_XLS
std::string ReportGenerator::Generator::WriteToExcel(std::shared_ptr<wpSQLResultSet> rs, const std::string &title, const std::string &subTitle, const std::string &tabName) {
    ExcelReader xlr(g_useXLSXformat);
    wxJSONValue jval;
    libxl::Sheet *sheet = CreateNewSheet(&xlr, rs->GetColumnCount(), tabName, title, subTitle);
    AppendToExcelSheet(&xlr, sheet, NULL, rs, jval, true);
    auto fileName = String::CreateTempFileName("kdr_");
    xlr.book->save(String::to_wstring(fileName).c_str());
    return fileName;
}
#endif

int64_t ReportGenerator::Generator::GetReturnData(std::shared_ptr<wpSQLResultSet> rs, PPOS::ReturnData *result, ConvertFunction fnConvert) {
    int *colDef = new int[rs->GetColumnCount()];
    memset(colDef, -1, rs->GetColumnCount());
    for (int i = 0; i < rs->GetColumnCount(); i++) {
        CreateColumnDefinition(colDef, rs, i);
    }
    int64_t nRows = 0;
    try {
        result->clear_row();
        for (; rs->NextRow(); nRows++) {
            auto row = result->add_row();
            for (int i = 0; i < rs->GetColumnCount(); i++) {
                std::string t = fnConvert(i, ConvertRowValue(colDef, rs, i));
                row->add_column(t);
            }
        }
    } catch (...) {
        LOG_ERROR("GetReturnData throw exception");
    }
    return nRows;
}

std::string ReportGenerator::Generator::ConvertRowValue(int *colDef, std::shared_ptr<wpSQLResultSet> rs, int i, std::function<std::string(std::shared_ptr<wpSQLResultSet>)> fn) {
    std::string t = fn ? fn(rs) : rs->Get(i);
    switch (colDef[i]) {
        case ColumnTypeTabDelim::DivFactor: {
            double x = rs->Get<double>(i);
            t = std::to_string(x);
            break;
        }
        case ColumnTypeTabDelim::date: {
            auto x = rs->Get<TimePoint>(i);
            if (IsValidTimePoint(x))
                t = FormatDate(x, "%d-%m-%Y");
            break;
        }
        case ColumnTypeTabDelim::year: {
            auto x = rs->Get<TimePoint>(i);
            t = FormatDate(x, "%Y");
            break;
        }
        case ColumnTypeTabDelim::month: {
            auto x = rs->Get<TimePoint>(i);
            if (IsValidTimePoint(x))
                t = FormatDate(x, "%b-%Y");
            break;
        }
        case ColumnTypeTabDelim::expiry: {
            auto x = rs->Get<TimePoint>(i);
            if (IsValidTimePoint(x))
                t = FormatDate(x, wpEXPIRYDATEFORMAT);
            break;
        }
        default:
            break;
    }
    return t;
}

void ReportGenerator::Generator::CreateColumnDefinition(int *colDef, std::shared_ptr<wpSQLResultSet> rs, int i) {
    colDef[i] = -1;
    std::string colName = rs->GetColumnName(i);
    if (boost::icontains(colName, "@keycode")) {
        colDef[i] = ColumnTypeTabDelim::KeyCode;
    } else if (boost::icontains(colName, "@dividefactor")) {
        colDef[i] = ColumnTypeTabDelim::DivFactor;
    } else if (boost::icontains(colName, "@date")) {
        colDef[i] = ColumnTypeTabDelim::date;
    } else if (boost::icontains(colName, "@year")) {
        colDef[i] = ColumnTypeTabDelim::year;
    } else if (boost::icontains(colName, "@month")) {
        colDef[i] = ColumnTypeTabDelim::month;
    } else if (boost::icontains(colName, "@expiry")) {
        colDef[i] = ColumnTypeTabDelim::expiry;
    }
}

std::string ReportGenerator::Generator::GetResultTabDelimited(std::shared_ptr<wpSQLResultSet> rs, int nRows, bool useActualTab, bool showColHeader, const std::string &filename) {
    std::string delim0;
    std::string delim;
    const std::string eolChar = useActualTab ? std::string("\r\n") : std::string(1, EOLINECHAR);
    const std::string eofChar = useActualTab ? std::string("\t") : std::string(1, EOFFIELDCHAR);

    // auto sout = std::make_unique<std::ostringstream>(std::ios_base::out);
    std::unique_ptr<std::ostream> out;
    if (filename.empty())
        out = std::make_unique<std::ostringstream>(std::ios_base::out);
    else
        out = std::make_unique<std::ofstream>(filename, std::ios_base::out);

    if (nRows >= 0) {
        *out << std::to_string(nRows) << eolChar;
    }
    int *colDef = new int[rs->GetColumnCount()];
    memset(colDef, -1, rs->GetColumnCount());
    //	std::vector<ColumnTypeTabDelim> colDef; colDef.resize(rs->GetColumnCount(), ColumnTypeTabDelim::None);
    for (int i = 0; i < rs->GetColumnCount(); i++) {
        CreateColumnDefinition(colDef, rs, i);
        if (showColHeader) {
            std::string str {rs->GetColumnName(i)};
            boost::tokenizer<boost::char_separator<char>> tok(str, boost::char_separator<char>("@", "", boost::keep_empty_tokens));
            auto it = tok.begin();
            std::string ttl = rs->GetColumnName(i);
            if (it != tok.end()) ttl = *it;

            *out << delim;
            *out << ttl;
            delim = eofChar;
        }
    }
    if (showColHeader) {
        delim0 = eolChar;
    }
    int nRow = 0;
    for (; rs->NextRow(); nRow++) {
        *out << delim0;
        delim = "";
        std::string rowRec;
        for (int i = 0; i < rs->GetColumnCount(); i++) {
            rowRec.append(delim);
            std::string t = ConvertRowValue(colDef, rs, i);
            if (useActualTab)
                rowRec.append(boost::trim_copy(t));
            else
                rowRec.append(t);
            delim = eofChar;
        }
        *out << rowRec;
        delim0 = eolChar;
    }
    delete[] colDef;
    if (filename.empty())
        return static_cast<std::ostringstream *>(out.get())->str();
    else
        return std::to_string(nRow);
}

std::vector<std::vector<std::string>> ReportGenerator::Generator::GetVectorResult(std::shared_ptr<wpSQLResultSet> rs, int nRows, bool showColHeader) {
    std::vector<std::vector<std::string>> result;
    try {
        int *colDef = new int[rs->GetColumnCount()];
        memset(colDef, -1, rs->GetColumnCount());
        std::vector<std::string> *currentRow {nullptr};
        if (showColHeader) currentRow = &result.emplace_back();
        for (int i = 0; i < rs->GetColumnCount(); i++) {
            CreateColumnDefinition(colDef, rs, i);
            if (showColHeader) {
                std::string str {rs->GetColumnName(i)};
                boost::tokenizer<boost::char_separator<char>> tok(str, boost::char_separator<char>("@", "", boost::keep_empty_tokens));
                auto ttl = rs->GetColumnName(i);
                auto it = tok.begin();
                if (it != tok.end()) ttl = *it;
                currentRow->emplace_back(ttl);
            }
        }
        int nRow = 0;
        for (; rs->NextRow(); nRow++) {
            currentRow = &result.emplace_back();
            for (int i = 0; i < rs->GetColumnCount(); i++) {
                currentRow->emplace_back(ConvertRowValue(colDef, rs, i));
            }
        }
        delete[] colDef;
    } catch (...) {
        LOG_ERROR("GetVectorResult throw exception");
    }
    return result;
}

} // namespace ReportGenerator

// CreateNewSection for HtmlReportBuilder — called from PdfOutputWriter for multi-section reports
void HtmlReportBuilder_CreateNewSection(HtmlReportBuilder &builder, DB::SQLiteBase &/*db*/, std::shared_ptr<wpSQLResultSet> rs, const std::string &orientation, const std::string &sName, const std::string &ttl, const std::string &sTtl, wxJSONValue &param) {
    LOG_INFO("HtmlReportBuilder_CreateNewSection");
    if (boost::iequals(orientation, "Landscape") || boost::iequals(orientation, "L"))
        builder.setOrientation("Landscape");
    else
        builder.setOrientation("Portrait");

    if (!sTtl.empty()) builder.setSubtitle(sTtl);

    const int nCol = rs->GetColumnCount();

    // Replace formatter
    auto *c = new DB::XLSColumnFormatter(NULL, NULL, rs, false, param);

    // Clear and rebuild column definitions
    builder.clearColumns();
    for (int i = 0; i < nCol; i++) {
        auto &cdef = *c->def()[i];
        builder.addColumn(
            String::to_string(cdef.colName),
            cdef.size > 0 ? cdef.size : 1.0,
            cdef.type == DB::XLSColumnFormatter::Number,
            String::to_string(cdef.sumFunction));
    }

    builder.setBreakPageOn(param.HasMember("break-page"));
    builder.setFormatterPtr(c);

    // Start a new section
    builder.newSection(sName);
}
