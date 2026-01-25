#pragma once
#ifndef NO_XLS
#include "libxl.h"
class ExcelReader;
#endif
#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <boost/json.hpp>
#include "guid.h"
#include "net.h"
#include "logger/logger.h"
#include "wpSQL/include/wpSQLDatabase.h"
#include "rDb.h"

extern bool g_useXLSXformat;

using ConvertFunction = std::function<std::string(int, const std::string &)>;

enum ColumnTypeTabDelim { None,
    KeyCode,
    DivFactor,
    date,
    year,
    month,
    expiry };

// erase nonprintable char : t.erase(std::remove_if(t.begin(), t.end(), [](unsigned char x) { return !std::isprint(x); }), t.end());
// replace std::replace_if(t.begin(), t.end(), [](unsigned char v) { return !std::isalnum(v); }, '.');


class ReportPDF;

namespace ReportGenerator {
    class Generator {
    public:
        using FunctionNonStaticMember = libxl::Sheet *(Generator::*)(ExcelReader *xlr, libxl::Sheet *sheet, std::shared_ptr<ReportPDF> rep, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader);
        using Function = libxl::Sheet *(*)(ExcelReader *xlr, libxl::Sheet *sheet, std::shared_ptr<ReportPDF> rep, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader);
    public:
        static Function GetProcessor(const std::string &cmdLine, wxJSONValue &param);
        static libxl::Sheet* CreateNewSheet(ExcelReader *xlr, int nCols, const std::string &sheetName, const std::string &title, const std::string &subTitle);
        static libxl::Sheet* AppendToExcelSheet(ExcelReader *xlr, libxl::Sheet *sheet, std::shared_ptr<ReportPDF>, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader);
        static libxl::Sheet* AppendToPDF(ExcelReader *xlsReader, libxl::Sheet *sheet, std::shared_ptr<ReportPDF> report, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader);
        static libxl::Sheet* AppendGroupingToExcelSheet(ExcelReader *xlr, libxl::Sheet *sheet, std::shared_ptr<ReportPDF>, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader);
        static std::string WriteToExcel(std::shared_ptr<wpSQLResultSet> rs, const std::string &title, const std::string &subTitle, const std::string &sheetName);
        static std::string WriteToExcel(std::vector<std::pair<std::shared_ptr<wpSQLResultSet>, std::vector<int>>> &rs, const std::string &title, const std::string &subTitle, const std::string &sheetName);
        static std::shared_ptr<ReportPDF> CreateNewPDF(std::shared_ptr<wpSQLResultSet> rs, const std::wstring &orientation, const std::wstring &sectionName, const std::wstring &title, const std::wstring &subTitle, wxJSONValue &param, const std::wstring outletName);
        //static wxJSONValue GetResultInJSON(std::shared_ptr<wpSQLResultSet> rs, std::unordered_map<std::string, std::function<std::string(std::shared_ptr<wpSQLResultSet>)>> fn);
        static std::string GetResultTabDelimited(std::shared_ptr<wpSQLResultSet> rs, bool useActualTab = false, bool showColumnHeader = false) { return GetResultTabDelimited(rs, -1, useActualTab, showColumnHeader); }
        static std::string GetResultTabDelimited(std::shared_ptr<wpSQLResultSet> rs, int nRows, bool useActualTab = false, bool showColumnHeader = false, const std::string &filename = "");
        static int64_t GetReturnData(std::shared_ptr<wpSQLResultSet> rs, PPOS::ReturnData *result, ConvertFunction fnConvert = [](int, const std::string s) { return s; });
        static std::vector<std::vector<std::string>> GetVectorResult(std::shared_ptr<wpSQLResultSet> rs, int nRows = -1, bool showColumnHeader = false);
        static std::string ConvertRowValue(int *colDef, std::shared_ptr<wpSQLResultSet> rs, int i, std::function<std::string(std::shared_ptr<wpSQLResultSet>)> fn = nullptr);
        static void CreateColumnDefinition(int *colDef, std::shared_ptr<wpSQLResultSet> rs, int i);
    };
}
