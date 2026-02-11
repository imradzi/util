#include "ReportOutputWriter.h"
#include "ExcelReader.h"
#include "PDFWriter.h"
#include "wpObject.h"
#include <boost/algorithm/string.hpp>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include "logger/logger.h"

// ─── ExcelOutputWriter ─────────────────────────────────────────────────────

ExcelOutputWriter::ExcelOutputWriter(bool useXLSXformat)
    : xlr_(useXLSXformat),
      genFn_((ReportGenerator::Generator::Function)&ReportGenerator::Generator::AppendToExcelSheet) {
}

void ExcelOutputWriter::CreateNewSection(DB::SQLiteBase& /*db*/,
                                         std::shared_ptr<wpSQLResultSet> rs,
                                         const std::string& /*orientation*/,
                                         int sheetNo,
                                         const std::string& name,
                                         const std::string& sel,
                                         wxJSONValue& /*param*/,
                                         const std::string& /*outletName*/) {
    sheet_ = ReportGenerator::Generator::CreateNewSheet(
        &xlr_, rs->GetColumnCount(), fmt::format("Sheet {}", sheetNo), name, sel);
}

void ExcelOutputWriter::AppendData(std::shared_ptr<wpSQLResultSet> rs,
                                   wxJSONValue& param,
                                   bool freezeHeader) {
    (*genFn_)(&xlr_, sheet_, nullptr, rs, param, freezeHeader);
}

void ExcelOutputWriter::ResetGenerator() {
    genFn_ = (ReportGenerator::Generator::Function)&ReportGenerator::Generator::AppendToExcelSheet;
}

void ExcelOutputWriter::SetGenerator(const std::string& directive, wxJSONValue& param) {
    genFn_ = ReportGenerator::Generator::GetProcessor(directive, param);
}

bool ExcelOutputWriter::AcceptsDirective(const std::string& directive) const {
    return boost::iequals(directive.substr(0, 8), "@ifExcel");
}

bool ExcelOutputWriter::HasSection() const {
    return sheet_ != nullptr;
}

std::wstring ExcelOutputWriter::Save(const std::wstring& baseName,
                                     bool dataExists,
                                     const std::string& noDataText,
                                     const std::string& /*orientation*/,
                                     const std::string& /*name*/,
                                     wxJSONValue& /*param*/,
                                     const std::string& /*outletName*/) {
    auto fileName = baseName + L".xls";
    if (!dataExists) {
        libxl::Sheet* shet = xlr_.book->addSheet(L"No Data");
        shet->writeStr(0, 1, L"No Data for:");
        shet->writeStr(1, 1, String::to_wstring(noDataText).c_str());
    }
    xlr_.book->save(fileName.c_str());
    LOG_INFO("Saved xls output to {}", String::to_string(fileName));
    return fileName;
}

// ─── PdfOutputWriter ───────────────────────────────────────────────────────

PdfOutputWriter::PdfOutputWriter()
    : genFn_((ReportGenerator::Generator::Function)&ReportGenerator::Generator::AppendToPDF) {
}

void PdfOutputWriter::CreateNewSection(DB::SQLiteBase& db,
                                       std::shared_ptr<wpSQLResultSet> rs,
                                       const std::string& orientation,
                                       int sheetNo,
                                       const std::string& name,
                                       const std::string& sel,
                                       wxJSONValue& param,
                                       const std::string& outletName) {
    if (!pdfReport_) {
        // First section: create a new PDF document.
        pdfReport_ = ReportGenerator::Generator::CreateNewPDF(
            rs,
            String::to_wstring(orientation),
            fmt::format(L"Section {}", sheetNo),
            String::to_wstring(name),
            String::to_wstring(sel),
            param,
            String::to_wstring(outletName));
    } else {
        // Subsequent sections: append to existing PDF.
        pdfReport_->CreateNewSection(db, rs, orientation,
            fmt::format("Section {}", sheetNo), name, sel, param);
    }
}

void PdfOutputWriter::AppendData(std::shared_ptr<wpSQLResultSet> rs,
                                 wxJSONValue& param,
                                 bool freezeHeader) {
    (*genFn_)(nullptr, nullptr, pdfReport_, rs, param, freezeHeader);
}

void PdfOutputWriter::ResetGenerator() {
    genFn_ = (ReportGenerator::Generator::Function)&ReportGenerator::Generator::AppendToPDF;
}

void PdfOutputWriter::SetGenerator(const std::string& directive, wxJSONValue& param) {
    genFn_ = ReportGenerator::Generator::GetProcessor(directive, param);
}

bool PdfOutputWriter::AcceptsDirective(const std::string& directive) const {
    return boost::iequals(directive.substr(0, 6), "@ifPDF");
}

bool PdfOutputWriter::HasSection() const {
    return pdfReport_ != nullptr;
}

std::wstring PdfOutputWriter::Save(const std::wstring& baseName,
                                   bool dataExists,
                                   const std::string& noDataText,
                                   const std::string& orientation,
                                   const std::string& name,
                                   wxJSONValue& param,
                                   const std::string& outletName) {
    auto fileName = baseName + L".pdf";
    if (!pdfReport_) {
        // No data at all — create a stub PDF so we still return a valid file.
        std::shared_ptr<wpSQLResultSet> rs;
        pdfReport_ = ReportGenerator::Generator::CreateNewPDF(
            rs, String::to_wstring(orientation),
            L"Section 1",
            String::to_wstring(name),
            String::to_wstring(noDataText),
            param,
            String::to_wstring(outletName));
    }
    if (!dataExists) {
        pdfReport_->AddPage();
        pdfReport_->Ln(20);
        pdfReport_->Cell(pdfReport_->GetPageWidth(), 0, "No Data for: ", 0, 1, wxPDF_ALIGN_CENTER);
        pdfReport_->Ln(10);
        pdfReport_->Cell(pdfReport_->GetPageWidth(), 0, noDataText, 0, 1, wxPDF_ALIGN_CENTER);
    }
    pdfReport_->SaveAsFile(fileName);
    return fileName;
}
