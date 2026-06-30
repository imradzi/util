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
#include "ppos.grpc.pb.h"
#include "ppos.pb.h"

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


class HtmlReportBuilder;

namespace ReportGenerator {
    class Generator {
    public:
        using FunctionNonStaticMember = libxl::Sheet *(Generator::*)(ExcelReader *xlr, libxl::Sheet *sheet, std::shared_ptr<HtmlReportBuilder> rep, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader);
        using Function = libxl::Sheet *(*)(ExcelReader *xlr, libxl::Sheet *sheet, std::shared_ptr<HtmlReportBuilder> rep, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader);
    public:
        static Function GetProcessor(const std::string &cmdLine, nlohmann::json &param);
        static libxl::Sheet* CreateNewSheet(ExcelReader *xlr, int nCols, const std::string &sheetName, const std::string &title, const std::string &subTitle);
<<<<<<< HEAD
        static libxl::Sheet* AppendToExcelSheet(ExcelReader *xlr, libxl::Sheet *sheet, std::shared_ptr<HtmlReportBuilder>, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader);
        static libxl::Sheet* AppendToPDF(ExcelReader *xlsReader, libxl::Sheet *sheet, std::shared_ptr<HtmlReportBuilder> report, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader);
        static libxl::Sheet* AppendGroupingToExcelSheet(ExcelReader *xlr, libxl::Sheet *sheet, std::shared_ptr<HtmlReportBuilder>, std::shared_ptr<wpSQLResultSet> rs, wxJSONValue &param, bool freezeHeader);
        static std::string WriteToExcel(std::shared_ptr<wpSQLResultSet> rs, const std::string &title, const std::string &subTitle, const std::string &sheetName);
        static std::shared_ptr<HtmlReportBuilder> CreateNewPDF(std::shared_ptr<wpSQLResultSet> rs, const std::wstring &orientation, const std::wstring &sectionName, const std::wstring &title, const std::wstring &subTitle, wxJSONValue &param, const std::wstring outletName);
        //static wxJSONValue GetResultInJSON(std::shared_ptr<wpSQLResultSet> rs, std::unordered_map<std::string, std::function<std::string(std::shared_ptr<wpSQLResultSet>)>> fn);
=======
        static libxl::Sheet* AppendToExcelSheet(ExcelReader *xlr, libxl::Sheet *sheet, std::shared_ptr<ReportPDF>, std::shared_ptr<wpSQLResultSet> rs, nlohmann::json &param, bool freezeHeader);
        static libxl::Sheet* AppendToPDF(ExcelReader *xlsReader, libxl::Sheet *sheet, std::shared_ptr<ReportPDF> report, std::shared_ptr<wpSQLResultSet> rs, nlohmann::json &param, bool freezeHeader);
        static libxl::Sheet* AppendGroupingToExcelSheet(ExcelReader *xlr, libxl::Sheet *sheet, std::shared_ptr<ReportPDF>, std::shared_ptr<wpSQLResultSet> rs, nlohmann::json &param, bool freezeHeader);
        static std::string WriteToExcel(std::shared_ptr<wpSQLResultSet> rs, const std::string &title, const std::string &subTitle, const std::string &sheetName);
        static void CreateColumnDefinition(int *colDef, std::shared_ptr<wpSQLResultSet> rs, int i);
    };
}

// Free function: create a new section on an HtmlReportBuilder (used by PdfOutputWriter)
void HtmlReportBuilder_CreateNewSection(HtmlReportBuilder &builder, DB::SQLiteBase &db, std::shared_ptr<wpSQLResultSet> rs, const std::string &orientation, const std::string &sName, const std::string &ttl, const std::string &sTtl, wxJSONValue &param);
