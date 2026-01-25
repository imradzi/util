#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

// Adapted from :
// Copyright 2007 Timo Bingmann <tb@panthema.net>
// Distributed under the Boost Software License, Version 1.0.
// (See http://www.boost.org/LICENSE_1_0.txt)

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
#include "wx/wfstream.h"
#include "wx/sstream.h"
#include "wx/zipstrm.h"
#include "wx/zstream.h"

#include <string>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include "global.h"
#include "words.h"

//#include "zlib.h"

//wxString CompressString(const wxString& str, int compressionlevel = Z_BEST_COMPRESSION) {
//	wxStringInputStream inStream(str);
//	wxStringOutputStream outStream;
//	wxZlibOutputStream zip(outStream, compressionlevel);
//	zip.Write(inStream);
//	zip.Close();
//	return outStream.GetString();
//}

//	z_stream zs;
//	memset(&zs, 0, sizeof(zs));
//
//	if (deflateInit(&zs, compressionlevel) != Z_OK)
//		throw std::exception("CompressString: inflateInit failed.");
//
//	zs.next_in = (Bytef *)(const char *)str.c_str();
//	zs.avail_in = str.length();
//
//	int ret;
//	char outbuffer[32768];
//
//	wxString outs;
//
//	do {
//		zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
//		zs.avail_out = sizeof(outbuffer);
//		ret = deflate(&zs, Z_FINISH);
//		if (outs.size() < zs.total_out)
//			outs.append(outbuffer, zs.total_out - outs.size());
//	} while (ret == Z_OK);
//
//	deflateEnd(&zs);
//
//	if (ret != Z_STREAM_END)
//		throw std::exception(wxString::Format("Exception during zlib compression: (%d) %s", ret , zs.msg).c_str());
//	return outs;
//}

//wxString CompressFile(const wxString& fileName, int compressionlevel = Z_BEST_COMPRESSION) {
//	wxFileInputStream ifs(fileName);
//	wxStringOutputStream s;
//	s.Write(ifs);
//	return CompressString(s.GetString());
//}

size_t CompressFile(const wxString &inName, const wxString &outName, int compressionlevel /*  = wxZ_BEST_COMPRESSION */) {
    if (!wxFileExists(inName)) return 0;
    wxFileInputStream fin(inName);
    if (!fin.IsOk()) return 0;
    wxFileOutputStream fout(outName);
    wxZlibOutputStream zip(fout, compressionlevel);
    zip.Write(fin);
    zip.Close();
    return fout.GetLength();
}

size_t UnCompressFile(const wxString &inName, const wxString &outName) {
    if (!wxFileExists(inName)) return 0;
    wxFileInputStream fin(inName);
    if (!fin.IsOk()) return 0;
    wxFileOutputStream fout(outName);
    wxZlibInputStream zip(fin);
    fout.Write(zip);
    return fout.GetFile()->Length();
}

/*

	if (!ifs.IsOk()) throw std::exception("CompressFile: file can't be opened");

	z_stream zs;
	memset(&zs, 0, sizeof(zs));

	if (deflateInit(&zs, compressionlevel) != Z_OK)
		throw std::exception("CompressString: inflateInit failed.");

	char buf[32768];
	int ret;
	char outbuffer[32768];
	int len = ifs.GetFile()->Length();
	//char *outs = new char[len];
	wxString returnstring;
	int lastPos=0;
	while (!ifs.Eof()) {
		zs.next_in = reinterpret_cast<Bytef*>(buf);
		zs.avail_in = ifs.Read(buf, sizeof(buf)).LastRead();
		zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
		zs.avail_out = sizeof(outbuffer);
		ret = deflate(&zs, Z_FINISH);
		//memcpy(outs+lastPos, outbuffer, zs.total_out - lastPos);
		returnstring.Append(outbuffer, zs.total_out - returnstring.length());
		lastPos = zs.total_out;
		if (ret != Z_OK) break;
	}
	deflateEnd(&zs);
	if (ret != Z_STREAM_END)
		throw std::exception(wxString::Format("Exception during zlib file compression: (%d) %s", ret , zs.msg).c_str());
	//wxString returnstring(outs, lastPos);
	//delete[] outs;
	return returnstring;
}
*/

//wxString UnCompressString(const wxString& str) {
//	wxStringInputStream fin(str);
//	wxStringOutputStream fout;
//	wxZlibInputStream zip(fin);
//	fout.Write(zip);
//	fout.Close();
//	return fout.GetString();
//}

//	z_stream zs;
//	memset(&zs, 0, sizeof(zs));
//
//	if (inflateInit(&zs) != Z_OK)
//		throw std::exception("UnCompressString: inflateInit failed.");
//
//	zs.next_in = (Bytef*)str.c_str().AsUnsignedChar();
//	zs.avail_in = str.size();
//
//	int ret;
//	char outbuffer[32768];
//	wxString outs;
//
//	do {
//		zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
//		zs.avail_out = sizeof(outbuffer);
//
//		ret = inflate(&zs, 0);
//
//		if (outs.size() < zs.total_out)
//			outs.append(outbuffer, zs.total_out - outs.size());
//
//	} while (ret == Z_OK);
//
//	inflateEnd(&zs);
//
//	if (ret != Z_STREAM_END)
//		throw std::exception(wxString::Format("Exception during zlib uncompress: (%d) %s", ret , zs.msg).c_str());
//	return outs;
//}

//wxString UnCompressFile(const wxString& fileName, int compressionlevel = Z_BEST_COMPRESSION) {
//	wxFileInputStream ifs(fileName);
//	wxStringOutputStream s;
//	ifs.Read(s);
//	return UnCompressString(s.GetString());
//}
