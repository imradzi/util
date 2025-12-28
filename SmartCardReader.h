#pragma once

#include <winscard.h>
#ifndef INFINITY
#define INFINITY 1e308 * 1e308
#endif

class SmartCardReader {
    static const unsigned char CmdSelectAppJPN[];
    static const unsigned char CmdAppResponse[];
    static const unsigned char CmdSetLength[];
    static const unsigned char CmdSelectFile[];
    static const unsigned char CmdGetData[];
    static const int fileLengths[];
    static SCARD_IO_REQUEST pciT0;

    void TrimString(unsigned char *out, unsigned char *in, int count);
    wxString Trim(unsigned char *in, int count);
    wxDateTime GetDate(unsigned char *in);
    wxString GetDateString(unsigned char *in);
    wxString GetPostCode(unsigned char *in);

    SCARDCONTEXT hSC;
    SCARDHANDLE hCard;
    std::vector<wxString> readerNames;

public:
    const std::vector<wxString> &GetReaders() { return readerNames; }
    MyKadData data;
    wxImage face;
    bool skipPhoto;
    static wxString GetErrorMessage(unsigned long errorCode);

public:
    SmartCardReader();
    ~SmartCardReader();
    bool Connect(int readerIndex);
    bool Disconnect();
    bool GetData();
    bool GetStatusAndWait(int readerIdx) { return GetStatus(readerIdx, INFINITY); }
    bool GetStatus(int readerIdx, double timeOut = 0);
};

int TestCARDReader(int reader_nb);
std::vector<wxString> GetSmartCardReaders();
