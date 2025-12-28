#pragma once
#include "wx/pdfdoc.h"
#include "rDb.h"
#include "SQLite3xlsFormatter.h"

#ifndef NO_XLS
#include "libxl.h"
#endif
#include <list>

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
        static wxJSONValue emptyJSON;
        std::vector<ColumnDefinition *> defArray;

    public:
        std::vector<ColumnDefinition *> &def() { return defArray; }
        double *data;

    public:
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

    public:
#ifndef NO_XLS
        XLSColumnFormatter(ExcelReader *xlReader, libxl::Sheet *xlsSheet, std::shared_ptr<wpSQLResultSet> resultSet, bool freezeHeader = false, wxJSONValue &param = emptyJSON);
#else
        XLSColumnFormatter(void *, void *, std::shared_ptr<wpSQLResultSet> resultSet, bool freezeHeader = false, wxJSONValue &param = emptyJSON);
#endif
        virtual ~XLSColumnFormatter();
    };
}

struct PDFColumnDefinition {
    std::wstring title;
    double w, h, weightage;
    int border, align;
    bool show;

public:
    PDFColumnDefinition(const std::wstring &ttl, double _weightage = 1.0, int _align = wxPDF_ALIGN_LEFT, int _border = wxPDF_BORDER_NONE, bool _show = true, double _h = 7.0) : 
        title(ttl),
        w(0.0),
        h(_h),
        weightage(_weightage),
        border(_border),
        align(_align),
        show(_show) {}
    
    PDFColumnDefinition() : title(L""),
                            w(0.0),
                            h(0.0),
                            weightage(0.0),
                            border(0),
                            align(0),
                            show(false) {}
};

class ReportPDF : public wxPdfDocument {
    std::wstring _title;

public:
    int x0, y0, xR, w0, h0;

public:
    std::wstring outletName;
    float startY;
    std::wstring subTitle, sectionName;
    std::wstring pageTitle;  // used only for breakOnPage
    std::wstring fontFace, fontType;
    std::vector<PDFColumnDefinition> *colDef;
    DB::XLSColumnFormatter *formatter;
    bool showFooter;
    bool breakPageOn;
    bool firstPagePrinted;
    int pageOrientation;
    std::function<bool()> fnIsLastLine;
    int lastLineBorder;
    bool showFooterPageNo;
    struct {
        int label, title, data, note, footer, total;
    } fontSize;

    unsigned char fillColorRed {0xCC}, fillColorGreen {0xFF}, fillColorBlue {0xFF};
    unsigned char boxColorRed {0x80}, boxColorGreen {0x80}, boxColorBlue {0x80};
    bool fillRect {true};
    bool letterheadFillRect {true};
    wxBitmap createQRBitmap(double x, double y, const std::wstring qrString);
public:
    ReportPDF(const std::wstring &title, const std::wstring outletName, int orientation = wxPORTRAIT);
    virtual ~ReportPDF();
    virtual void CreateNewSection(DB::SQLiteBase &db, std::shared_ptr<wpSQLResultSet> rs, const std::string &orientation, const std::string &section, const std::string &title, const std::string &subTitlesel, wxJSONValue &param);

    double GetFontHeight() const;
    void ComputeColumnWeightage(std::vector<PDFColumnDefinition> *p, int tabStart);
    void ComputeColumnWeightage(int tabStart) { ComputeColumnWeightage(colDef, tabStart); }
    void RenderData(std::vector<PDFColumnDefinition> *p, wpSQLResultSet &rs);
    void RenderData(std::vector<PDFColumnDefinition> *p, const std::vector<std::wstring> &data);
    void RenderData(wpSQLResultSet &rs) { RenderData(colDef, rs); }
    void RenderData(const std::vector<std::wstring> &data) { RenderData(colDef, data); }
    void RenderHeader(std::vector<PDFColumnDefinition> *p, const std::wstring addLine = L"");
    void DrawBoundingRect() { Rect(GetLeftMargin(), GetTopMargin(), GetPageWidth() - (GetLeftMargin() + GetRightMargin()), GetPageHeight() - (GetTopMargin() * 2)); }

    void Header();
    void Footer();
    void ShowGrandTotal();
    void ResetMargins() { SetMargins(x0, y0, xR); }
    void GoToRightMost() { SetX(x0); }
    void DrawFullLine();

    std::tuple<double, int> WriteBulletPoints(double x, double y, double w, double lineHeight, const std::wstring &title, std::vector<std::wstring> &points, double h = 0);                // return height;
    std::tuple<double, int> WriteList(double x, double y, double w, double lineHeight, const std::wstring &title, std::vector<std::wstring> &points, double h = 0);                        // return height;
    std::tuple<double, int> WriteLetterHead(const std::wstring &name, const std::wstring &name2, const std::wstring &address, const std::wstring &regNo, const std::wstring &imageFileName, const std::wstring &eInvoiceQRstring);  // return lineNo where the below letterhead
    void WriteBox(double x, double y, const std::wstring &title, const std::vector<std::wstring> &lines, double rightAlignTo = 0);
};
