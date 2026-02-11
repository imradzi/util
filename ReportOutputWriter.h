#pragma once
#include <memory>
#include <string>
#include "jsonval.h"
#include "ExcelReader.h"
#include "SQLite3xlsFormatter.h"

namespace DB { class SQLiteBase; }

// Abstract interface for report output (PDF or Excel).
// Eliminates scattered isExcel/isPDF branching in ExecuteCommand.
class ReportOutputWriter {
public:
    virtual ~ReportOutputWriter() = default;

    // Create a new section/sheet. Called when sheet_type is "multi" or on first result set.
    virtual void CreateNewSection(DB::SQLiteBase& db,
                                  std::shared_ptr<wpSQLResultSet> rs,
                                  const std::string& orientation,
                                  int sheetNo,
                                  const std::string& name,
                                  const std::string& sel,
                                  wxJSONValue& param,
                                  const std::string& outletName) = 0;

    // Append data rows to the current section/sheet.
    virtual void AppendData(std::shared_ptr<wpSQLResultSet> rs,
                            wxJSONValue& param,
                            bool freezeHeader) = 0;

    // Reset the generator function pointer back to default for this output type.
    virtual void ResetGenerator() = 0;

    // Set a custom generator from a script directive (e.g. @groupBy).
    virtual void SetGenerator(const std::string& directive, wxJSONValue& param) = 0;

    // Returns true if this writer should accept lines when the given directive is active.
    // e.g. "@ifPDF" -> true for PdfOutputWriter, "@ifExcel" -> true for ExcelOutputWriter.
    virtual bool AcceptsDirective(const std::string& directive) const = 0;

    // Returns true if a section/sheet has been started (i.e. CreateNewSection was called at least once).
    virtual bool HasSection() const = 0;

    // Save the report to a file. Returns the full file path with extension.
    // Handles the "no data" case internally.
    virtual std::wstring Save(const std::wstring& baseName,
                              bool dataExists,
                              const std::string& noDataText,
                              const std::string& orientation,
                              const std::string& name,
                              wxJSONValue& param,
                              const std::string& outletName) = 0;
};

// ─── Excel Writer ───────────────────────────────────────────────────────────

class ExcelOutputWriter : public ReportOutputWriter {
    ExcelReader xlr_;
    libxl::Sheet* sheet_ = nullptr;
    ReportGenerator::Generator::Function genFn_;

public:
    explicit ExcelOutputWriter(bool useXLSXformat);

    void CreateNewSection(DB::SQLiteBase& db,
                          std::shared_ptr<wpSQLResultSet> rs,
                          const std::string& orientation,
                          int sheetNo,
                          const std::string& name,
                          const std::string& sel,
                          wxJSONValue& param,
                          const std::string& outletName) override;

    void AppendData(std::shared_ptr<wpSQLResultSet> rs,
                    wxJSONValue& param,
                    bool freezeHeader) override;

    void ResetGenerator() override;
    void SetGenerator(const std::string& directive, wxJSONValue& param) override;
    bool AcceptsDirective(const std::string& directive) const override;
    bool HasSection() const override;

    std::wstring Save(const std::wstring& baseName,
                      bool dataExists,
                      const std::string& noDataText,
                      const std::string& orientation,
                      const std::string& name,
                      wxJSONValue& param,
                      const std::string& outletName) override;
};

// ─── PDF Writer ─────────────────────────────────────────────────────────────

class PdfOutputWriter : public ReportOutputWriter {
    std::shared_ptr<ReportPDF> pdfReport_;
    ReportGenerator::Generator::Function genFn_;

public:
    PdfOutputWriter();

    void CreateNewSection(DB::SQLiteBase& db,
                          std::shared_ptr<wpSQLResultSet> rs,
                          const std::string& orientation,
                          int sheetNo,
                          const std::string& name,
                          const std::string& sel,
                          wxJSONValue& param,
                          const std::string& outletName) override;

    void AppendData(std::shared_ptr<wpSQLResultSet> rs,
                    wxJSONValue& param,
                    bool freezeHeader) override;

    void ResetGenerator() override;
    void SetGenerator(const std::string& directive, wxJSONValue& param) override;
    bool AcceptsDirective(const std::string& directive) const override;
    bool HasSection() const override;

    std::wstring Save(const std::wstring& baseName,
                      bool dataExists,
                      const std::string& noDataText,
                      const std::string& orientation,
                      const std::string& name,
                      wxJSONValue& param,
                      const std::string& outletName) override;
};
