#pragma once
#include "rDb.h"
#include "SQLite3xlsFormatter.h"

#ifndef NO_XLS
#include "libxl.h"
#endif
#include <list>
#include <map>

#include <wx/numformatter.h>

namespace DB {
    class Expression;

    extern std::unique_ptr<DB::Expression> CreateExpression(const std::wstring &s, int nCols);
    extern bool SetVector(DB::Expression *e, double *vec);
    extern double ExecuteExpression(DB::Expression *e);

    class XLSColumnFormatter {
    public:
        enum ColumnType { Number,
            Month,
            WeekDay,
            Date,
            DateTime,
            Time,
            String };
        struct ColumnDefinition {
#ifndef NO_XLS
            libxl::Format *fmt;
            libxl::Format *fmtHighlight;
#else
            int *fmt;
            int *fmtHighlight;
#endif
            bool subTotalLabel;
            ColumnType type;
            std::wstring sumFunction;
            std::wstring colName;
            std::wstring formula;
            int length, precision;
            bool isDivideFactor;
            double offset;
            double size;
            double pageTotal, grandTotal, nRecTotal;
            long nRecPage;
            std::unique_ptr<DB::Expression> expression;
            std::unique_ptr<DB::Expression> footerExpression;

        public:
            ColumnDefinition();
            ColumnDefinition(const ColumnDefinition &c) = delete;
        };

    private:
        std::shared_ptr<wpSQLResultSet> rs;
#ifndef NO_XLS
        ExcelReader *xlr;
        libxl::Sheet *sheet;
#endif
        static nlohmann::json emptyJSON;
        std::vector<ColumnDefinition *> defArray;
#ifndef NO_XLS
        std::map<libxl::Format *, libxl::Format *> wrapFormats_;
#endif

    public:
        std::vector<ColumnDefinition *> &def() { return defArray; }
        double *data;

    public:
        void SetResultSet(std::shared_ptr<wpSQLResultSet> newRs) { rs = newRs; }

#ifndef NO_XLS
        libxl::Format *GetXLSFormat(int i, bool highlight = false) {
            assert(i >= 0 && i < int(defArray.size()));
            return highlight ? defArray[i]->fmtHighlight : defArray[i]->fmt;
        }
#endif
        bool IsNumber(int i) {
            assert(i >= 0 && i < int(defArray.size()));
            return defArray[i]->type == Number;
        }
        bool IsFormula(int i) {
            assert(i >= 0 && i < int(defArray.size()));
            return !defArray[i]->formula.empty();
        }
        int GetColumnSize(int i) {
            assert(i >= 0 && i < int(defArray.size()));
            return defArray[i]->length;
        }
        std::wstring GetSumFunction(int i) {
            assert(i >= 0 && i < int(defArray.size()));
            return defArray[i]->sumFunction;
        }
        std::wstring GetString(int i, bool toFormatNumber = false);
        double GetDouble(int i);
        wxLongLong GetValue(int i);
        virtual void WriteString(long row, int col, int i, bool isHighlight = false);

        // Replaces <br>, <br/> and <br /> tags with newlines (returns true if anything
        // was replaced). Lets headings/cells that carry explicit HTML line breaks
        // for PDF output render multi-line in Excel as well.
        static bool ReplaceBrWithNewline(std::wstring &s);
#ifndef NO_XLS
        // Returns a wrap-enabled copy of the given format (cached per base format).
        // Excel requires the wrap flag to display an embedded '\n' as a line break.
        libxl::Format *GetWrapFormat(libxl::Format *base);
        // writeStr for Excel output cells: converts <br> tags to newlines and uses
        // a wrap-enabled format when a replacement happened.
        void WriteCell(libxl::Sheet *sh, long row, int col, const std::wstring &value, libxl::Format *fmt);
#endif

    public:
#ifndef NO_XLS
        XLSColumnFormatter(ExcelReader *xlReader, libxl::Sheet *xlsSheet, std::shared_ptr<wpSQLResultSet> resultSet, bool freezeHeader = false, nlohmann::json &param = emptyJSON);
#else
        XLSColumnFormatter(void *, void *, std::shared_ptr<wpSQLResultSet> resultSet, bool freezeHeader = false, nlohmann::json &param = emptyJSON);
#endif
        virtual ~XLSColumnFormatter();
    };
}
