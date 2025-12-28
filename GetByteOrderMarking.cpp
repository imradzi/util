#include "precompiled/libcommon.h"
#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
#ifdef _WIN32
#include "winsock2.h"
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
#include "wx/sstream.h"
#include "wx/wfstream.h"

/*
From Wikipedia page on Byte Order Marking: http://en.wikipedia.org/wiki/Byte_order_mark

Encoding			Representation (hexadecimal)
UTF-8				EF BB BF
UTF-16 (BE)			FE FF
UTF-16 (LE)			FF FE
UTF-32 (BE)			00 00 FE FF
UTF-32 (LE)			FF FE 00 00
UTF-7				2B 2F 76, and one of the following bytes: [ 38 | 39 | 2B | 2F ]
UTF-1				F7 64 4C
UTF-EBCDIC			DD 73 66 73
SCSU				0E FE FF
BOCU-1				FB EE 28 optionally followed by FF
GB-18030			84 31 95 33
*/

namespace String {
    bool IsUnicodeText(const wxString &filename) {
        char buf[200];
        wxFile f(filename);

        ssize_t sz = f.Read(buf, 200);
        const char *p = buf;
        bool prevZero = false;
        for (ssize_t i = 0; i < sz; i++, p++) {
            if (*p == 0) {
                if (prevZero && i > 0) return false;
                prevZero = true;
            } else {
                if (!prevZero && i > 0) return false;
                prevZero = false;
            }
        }
        return true;
    }

    struct FileTypes {
        char signature[10];
        int sz;
        char typeName[20];
        char nextChar[20];
    };

    static FileTypes _fileTypeList[] = {
        {"\xEF\xBB\xBF\x0", 3, "UTF-8", "\0"},
        {"\xFE\xFF\x0\x0", 2, "UTF-16 (BE)", "\0"},
        {"\xFF\xFE", 2, "UTF-16 (LE)", "\0"},
        {"\x00\x00\xFE\xFF", 4, "UTF-32 (BE)", "\0"},
        {"\xFF\xFE\x00\x00", 4, "UTF-32 (LE)", "\0"},
        {"\x2B\x2F\x76", 3, "UTF-7", "\x38\x39\x2B\x2F\x0"},  //and one of the following bytes: [ 38 | 39 | 2B | 2F ]
        {"\xF7\x64\x4C", 3, "UTF-1", "\0"},
        {"\xDD\x73\x66\x73", 4, "UTF-EBCDIC", "\0"},
        {"\x0E\xFE\xFF", 3, "SCSU", "\0"},
        {"\xFB\xEE\x28", 3, "BOCU-1", "\xFF\x0"},  //optionally followed by FF
        {"\x84\x31\x95\x33", 4, "GB-18030", "\0"}};

    wxString GetFileType(const wxString &filename, int &noToSkip) {
        wxFile f(filename);
        char buf[10];
        ssize_t sz = f.Read(buf, sizeof(buf));
        noToSkip = 0;
        if (sz == sizeof(buf)) {
            FileTypes *ft = _fileTypeList;
            for (unsigned int i = 0; i < sizeof(_fileTypeList) / sizeof(FileTypes); i++, ft++) {
                if (memcmp(ft->signature, buf, ft->sz) == 0) {
                    int incr = 0;
                    char *nchar = buf + ft->sz;
                    for (char *p = ft->nextChar; *p; p++) {
                        if (*nchar == *p) {
                            incr = 1;
                            break;
                        }
                    }
                    noToSkip = ft->sz + incr;
                    return ft->typeName;
                }
            }
        }
        return "";
    }

    wxString LoadFileIntoString(const wxString &fName) {
        int noToSkip = 0;
        wxString fileType = String::GetFileType(fName, noToSkip);
        wxString cmd;
        if (fileType.Mid(0, 5).IsSameAs("UTF-8")) {
            wxFileInputStream inpf(fName);
            wxStringOutputStream s;
            inpf.SeekI(noToSkip);
            s.Write(inpf);
            cmd = s.GetString();
        } else if (fileType.Mid(0, 6).IsSameAs("UTF-16")) {
            wxChar buf[4096];
            memset(buf, 0, sizeof(buf));
            wxFileInputStream inpf(fName);
            inpf.SeekI(noToSkip);
            while (!inpf.Eof()) {
                inpf.Read((void *)buf, sizeof(buf));
                cmd.Append(buf, inpf.LastRead() / sizeof(wxChar));
            }
        } else if (fileType.IsEmpty()) {
            if (String::IsUnicodeText(fName)) {
                wxChar buf[4096];
                memset(buf, 0, sizeof(buf));
                wxFileInputStream inpf(fName);
                inpf.SeekI(noToSkip);
                while (!inpf.Eof()) {
                    inpf.Read((void *)buf, sizeof(buf));
                    cmd.Append(buf, inpf.LastRead() / sizeof(wxChar));
                }
            } else {
                wxFileInputStream inpf(fName);
                wxStringOutputStream s;
                inpf.SeekI(noToSkip);
                s.Write(inpf);
                cmd = s.GetString();
            }
        }
        return cmd;
    }
}