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
libxl::Sheet *ReportGenerator::Generator::AppendGroupingToExcelSheet(ExcelReader *xlsReader, libxl::Sheet *sheet, std::shared_ptr<ReportPDF> /*unused*/, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader) {
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

libxl::Sheet *ReportGenerator::Generator::AppendToExcelSheet(ExcelReader *xlsReader, libxl::Sheet *sheet, std::shared_ptr<ReportPDF> /*unused*/, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader) {
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
            LOG_INFO("libxl.version={}/{}> Error {}: [{}]", xlr.book->version(), xlr.book->biffVersion(), xlr.book->errorMessage(), x);
        }
    }
    return sheet;
}
#endif

std::shared_ptr<ReportPDF> ReportGenerator::Generator::CreateNewPDF(std::shared_ptr<wpSQLResultSet> rs, const std::wstring &orientation, const std::wstring &sectionName, const std::wstring &title, const std::wstring &subTitle, wxJSONValue &param, const std::wstring outletName) {
    LOG_INFO("CreateNewPDF");
    int pageOrientation = wxPORTRAIT;
    if (boost::iequals(orientation, "Landscape")) pageOrientation = wxLANDSCAPE;
    if (boost::iequals(orientation, "L")) pageOrientation = wxLANDSCAPE;
    auto pdf = std::make_shared<ReportPDF>(title, outletName, pageOrientation);
    pdf->subTitle = String::to_wstring(subTitle);
    pdf->sectionName = String::to_wstring(sectionName);
    pdf->SetAuthor("PharmaPOS");
    pdf->SetTitle(title);
    pdf->SetFont("Arial", "B", 8);
    pdf->SetAutoPageBreak(false);
    if (rs == NULL) return pdf;

    const int nCol = rs->GetColumnCount();
    DB::XLSColumnFormatter *c = pdf->formatter = new DB::XLSColumnFormatter(NULL, NULL, rs, false, param);
    if (!param.HasMember("column-sizes")) {
        // while (rs->NextRow()) {
        //	for (int i = 0; i < nCol; i++) {
        //		std::string v = c->GetString(i, true);
        //		if (v.length() > c->def()[i]->size) c->def()[i]->size = v.length();
        //	}
        // }
        for (int i = 0; i < nCol; i++)
            c->def()[i]->size = 1;
    }

    pdf->breakPageOn = param.HasMember("break-page");

    double totLength = 0;
    for (int i = (pdf->breakPageOn ? 1 : 0); i < nCol; i++) {
        totLength += c->def()[i]->size;
    }

    double w = pdf->GetPageWidth() - pdf->GetRightMargin() - pdf->GetLeftMargin();
    for (int i = (pdf->breakPageOn ? 1 : 0); i < nCol; i++) {
        c->def()[i]->size = double(c->def()[i]->size) / totLength * w;
    }
    // pdf->AddPage(pageOrientation);
    return pdf;
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

// #ifdef NO_XLS
libxl::Sheet *ReportGenerator::Generator::AppendToPDF(ExcelReader *, libxl::Sheet *, std::shared_ptr<ReportPDF> report, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool /*freezeHeader*/) {
// #else
// libxl::Sheet *ReportGenerator::Generator::AppendToPDF(std::shared_ptr<ReportPDF> report, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool /*freezeHeader*/) {
// #endif

    if (!report) throw std::runtime_error("AppendToPDF: report is NULL!!!");

    ReportPDF &pdf(*report);
    pdf.SetFont("Arial", "", 7);
    DB::XLSColumnFormatter *fmt = pdf.formatter;
    int lineheight = param.HasMember("lineheight") ? param["lineheight"].AsInt() : 5;
    std::string fontName = param.HasMember("fontname") ? std::string(param["fontname"].AsString()) : "";
    std::string fontType = param.HasMember("fonttype") ? std::string(param["fonttype"].AsString()) : "";
    int fontSize = param.HasMember("fontsize") ? param["fontsize"].AsInt() : 7;
    if (!fontName.empty())
        pdf.SetFont(fontName, fontType, fontSize);
    bool formulaExists = false;
    const int nCol = rs->GetColumnCount();
    for (int i = 0; i < nCol; i++) {
        DB::XLSColumnFormatter::ColumnDefinition &cdef = *fmt->def()[i];
        if (cdef.expression) {
            formulaExists = true;
            break;
        }
    }

    std::vector<bool> keyColumns;
    keyColumns.resize(nCol);
    std::vector<std::string> prevValues;
    prevValues.resize(nCol);
    std::vector<std::string> currValues;
    currValues.resize(nCol);
    std::vector<std::vector<std::string>> buffer;
    buffer.resize(nCol);

    std::vector<bool> toCheckDup;
    toCheckDup.resize(nCol);
    if (param.HasMember("removedup")) {
        for (unsigned int i = 0; i < param["removedup"].Size(); i++) {
            int j = param["removedup"][i].AsInt();
            if (j >= 0) toCheckDup[j] = true;
        }
    }
    int startOfs = (pdf.breakPageOn ? 1 : 0);
    bool compareKey = false;
    bool showAllKeys = param.HasMember("show-all-keys") && (param["show-all-keys"].AsBool());
    if (param.HasMember("key-columns")) {
        for (unsigned int i = 0; i < param["key-columns"].Size(); i++) {
            int j = param["key-columns"][i].AsInt();
            if (j >= 0) keyColumns[j] = true;
            compareKey = true;
        }
    }

    bool isSameKey = false;

    auto fnFlushBuffer = [&]() -> bool {
        bool lineExist = false;
        std::vector<std::string> prev(nCol);
        for (; true;) {
            bool hasBuffer = false;
            std::vector<std::string> toPrint;
            toPrint.resize(nCol);
            for (int i = 0; i < nCol; i++) {
                std::vector<std::string> &b = buffer[i];
                std::vector<std::string>::iterator it = b.begin();
                if (it != b.end()) {
                    toPrint[i] = *it;
                    // if (toPrint[i] == prev[i] && keyColumns[i]) toPrint[i] = "";
                    prev[i] = *it;
                    b.erase(it);
                    hasBuffer |= true;
                }
            }
            if (hasBuffer) {
                lineExist = true;
                for (int i = startOfs; i < nCol; i++) {
                    std::string v = toPrint[i];
                    int just = wxPDF_ALIGN_LEFT;
                    DB::XLSColumnFormatter::ColumnDefinition &cdef = *fmt->def()[i];
                    if (cdef.type == DB::XLSColumnFormatter::Number) {
                        just = wxPDF_ALIGN_RIGHT;
                    }
                    pdf.ClippedCell(cdef.size, lineheight, v, wxPDF_BORDER_TOP | wxPDF_BORDER_BOTTOM | wxPDF_BORDER_LEFT | wxPDF_BORDER_RIGHT, 0, just);
                }
                pdf.Ln();
            } else
                break;
        }

        buffer.clear();
        buffer.resize(nCol);
        if (lineExist) {
            pdf.Ln();
            if (pdf.GetY() >= pdf.GetPageHeight() - 20) {
                pdf.AddPage();
            }
        }
        return lineExist;
    };

    auto fnRemoveLastWord = [](std::string &s) -> std::string {
        auto idx = s.find_last_of(' ');
        if (idx != std::string::npos) {
            auto bal = s.substr(idx + 1);
            s.resize(idx);
            return bal;
        }
        std::string bal = s;
        s.clear();
        return bal;
    };

    while (rs->NextRow()) {
        if (pdf.breakPageOn) {
            auto v = String::to_wstring(fmt->GetString(0));
            if (!boost::equals(v, pdf.pageTitle)) {
                fnFlushBuffer();
                pdf.pageTitle = v;
                pdf.AddPage(pdf.pageOrientation);
            }
        }
        if (!pdf.firstPagePrinted) pdf.AddPage(pdf.pageOrientation);

        if (formulaExists) {
            for (int i = 0; i < nCol; i++) {
                DB::XLSColumnFormatter::ColumnDefinition &cdef = *fmt->def()[i];
                if (cdef.type == DB::XLSColumnFormatter::Number && fmt->data)
                    fmt->data[i] = fmt->GetDouble(i);
            }
        }
        if (compareKey) {
            for (int i = 0; i < nCol; i++) {
                std::string v = String::to_string(fmt->GetString(i, true));
                currValues[i] = v;
            }
            isSameKey = IsSameKey(keyColumns, prevValues, currValues);
        }
        if (compareKey && !isSameKey) {
            fnFlushBuffer();
            prevValues.clear();
            prevValues.resize(nCol);
        }
        for (int i = startOfs; i < nCol; i++) {
            auto &cdef = *fmt->def()[i];
            std::string v = String::to_string(fmt->GetString(i, true));
            bool valueChanges = true;
            if (compareKey) {
                if (keyColumns[i]) {
                    currValues[i] = v;
                    valueChanges = (v != prevValues[i]);
                    if (valueChanges) prevValues[i] = v;
                }
                if (!valueChanges) continue;
                if (boost::contains(v, "\n")) {
                    wxStringTokenizer tk(v, "\n");
                    while (tk.HasMoreTokens()) {
                        std::string s(tk.GetNextToken());
                        boost::trim(s);
                        auto w = pdf.GetStringWidth(s);
                        if (w > cdef.size) {
                            std::string leftover;
                            while (true) {
                                while (pdf.GetStringWidth(s) > cdef.size) {
                                    auto le = fnRemoveLastWord(s);
                                    if (s.empty()) {
                                        s = le;
                                        break;
                                    }
                                    leftover.insert(0, " ");
                                    leftover.insert(0, le);
                                }
                                if (!s.empty()) buffer[i].emplace_back(s);
                                if (leftover.empty()) break;
                                s = boost::trim_copy(leftover);
                                leftover.clear();
                            }
                        } else
                            buffer[i].emplace_back(s);
                    }
                } else
                    buffer[i].emplace_back(v);
                continue;
            }
            int just = wxPDF_ALIGN_LEFT;
            if (cdef.type == DB::XLSColumnFormatter::Number) {
                just = wxPDF_ALIGN_RIGHT;
                double t = fmt->GetDouble(i);
                cdef.pageTotal += t;
                cdef.grandTotal += t;
                cdef.nRecPage++;
                cdef.nRecTotal++;
            }

            if (boost::contains(v, "\n") && valueChanges) {
                buffer[i] = std::vector<std::string>();  // empty string list;
                wxStringTokenizer tk(v, "\n");
                if (tk.HasMoreTokens()) {
                    for (int idx = 0; tk.HasMoreTokens(); idx++) {
                        std::string s(tk.GetNextToken());
                        if (idx == 0)
                            pdf.ClippedCell(cdef.size, lineheight, s, wxPDF_BORDER_TOP | wxPDF_BORDER_BOTTOM | wxPDF_BORDER_LEFT | wxPDF_BORDER_RIGHT, 0, just);
                        else
                            buffer[i].emplace_back(s);
                    }
                } else {
                    pdf.ClippedCell(cdef.size, lineheight, "", wxPDF_BORDER_TOP | wxPDF_BORDER_BOTTOM | wxPDF_BORDER_LEFT | wxPDF_BORDER_RIGHT, 0, just);
                }
            } else {
                if (!valueChanges && !isSameKey && keyColumns[i] && showAllKeys) {
                    pdf.ClippedCell(cdef.size, lineheight, v, wxPDF_BORDER_TOP | wxPDF_BORDER_BOTTOM | wxPDF_BORDER_LEFT | wxPDF_BORDER_RIGHT, 0, just);
                } else {
                    pdf.ClippedCell(cdef.size, lineheight, valueChanges ? v : std::string(), wxPDF_BORDER_TOP | wxPDF_BORDER_BOTTOM | wxPDF_BORDER_LEFT | wxPDF_BORDER_RIGHT, 0, just);
                }
            }
        }
        if (!compareKey) {
            pdf.Ln();
            if (pdf.GetY() >= pdf.GetPageHeight() - 20) {
                pdf.AddPage();
            }
        }
    }
    for (; true;) {
        bool hasBuffer = false;
        std::vector<std::string> toPrint;
        toPrint.resize(nCol);
        for (int i = 0; i < nCol; i++) {
            std::vector<std::string> &b = buffer[i];
            std::vector<std::string>::iterator it = b.begin();
            if (it != b.end()) {
                toPrint[i] = *it;
                b.erase(it);
                hasBuffer |= true;
            }
        }
        if (hasBuffer) {
            for (int i = startOfs; i < nCol; i++) {
                std::string v = toPrint[i];
                int just = wxPDF_ALIGN_LEFT;
                DB::XLSColumnFormatter::ColumnDefinition &cdef = *fmt->def()[i];
                if (cdef.type == DB::XLSColumnFormatter::Number) {
                    just = wxPDF_ALIGN_RIGHT;
                }
                pdf.ClippedCell(cdef.size, lineheight, v, wxPDF_BORDER_TOP | wxPDF_BORDER_BOTTOM | wxPDF_BORDER_LEFT | wxPDF_BORDER_RIGHT, 0, just);
            }
            pdf.Ln();
        } else
            break;
    }

    // double lm = pdf.GetLeftMargin();
    pdf.Footer();
    pdf.showFooter = false;
    pdf.Ln();
    pdf.ShowGrandTotal();
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

void ReportPDF::CreateNewSection(DB::SQLiteBase &db, std::shared_ptr<wpSQLResultSet> rs, const std::string &orientation, const std::string &sName, const std::string &ttl, const std::string &sTtl, wxJSONValue &param) {
    LOG_INFO("CreateNewSection");
    pageOrientation = wxPORTRAIT;
    if (boost::iequals(orientation, "Landscape")) pageOrientation = wxLANDSCAPE;
    if (boost::iequals(orientation, "L")) pageOrientation = wxLANDSCAPE;

    if (!sTtl.empty()) subTitle = String::to_wstring(sTtl);
    if (!sName.empty()) sectionName = String::to_wstring(sName);
    if (!ttl.empty()) SetTitle(ttl);

    const int nCol = rs->GetColumnCount();

    SetFont("Arial", "B", 8);
    SetAutoPageBreak(false);
    if (formatter) delete formatter;
    DB::XLSColumnFormatter *c = formatter = new DB::XLSColumnFormatter(NULL, NULL, rs, false, param);
    if (!param.HasMember("column-sizes")) {
        while (rs->NextRow()) {
            for (int i = 0; i < nCol; i++) {
                auto v = c->GetString(i, true);
                if (v.length() > c->def()[i]->size) c->def()[i]->size = v.length();
            }
        }
        for (int i = 0; i < nCol; i++) {
            if (boost::iequals(c->def()[i]->sumFunction, "sum"))
                c->def()[i]->size += 3;  // add 2 digit for total
        }
    }

    breakPageOn = param.HasMember("break-page");

    double totLength = 0;
    for (int i = (breakPageOn ? 1 : 0); i < nCol; i++) {
        totLength += c->def()[i]->size;
    }

    double w = GetPageWidth() - GetRightMargin() - GetLeftMargin();
    for (int i = (breakPageOn ? 1 : 0); i < nCol; i++) {
        c->def()[i]->size = double(c->def()[i]->size) / totLength * w;
    }
    AddPage(pageOrientation);
    showFooter = true;
}
