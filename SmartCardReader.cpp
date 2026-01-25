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
#include "wx/socket.h"
#include "wx/wfstream.h"
#include "wx/txtstrm.h"
#include "wx/mstream.h"

#include "global.h"
#include "words.h"
#include "wpObject.h"

#include <list>
#include <vector>
#include <winscard.h>
#include "globalfunction.h"

#include "SmartCardReader.h"


#ifdef _WIN32
#pragma comment(lib, "winscard.lib")
#endif

void SmartCardReader::TrimString(unsigned char *out, unsigned char *in, int count) {
    int i, j = 0;
    for (i = count - 1; i >= 0; i--) {
        if (in[i] != ' ') break;
    }
    for (j = 0; j <= i; j++) {
        out[j] = in[j];
    }
    out[j] = '\0';
}

wxString SmartCardReader::Trim(unsigned char *in, int count) {
    int i;
    for (i = count - 1; i >= 0; i--) {
        if (in[i] != ' ') break;
    }
    return wxString(in, i + 1);
}

wxDateTime SmartCardReader::GetDate(unsigned char *in) {
    wxString d = GetDateString(in);
    wxDateTime dt;
    dt.ParseDate(d);
    return dt;
}
wxString SmartCardReader::GetDateString(unsigned char *in) {
    wxString res;
    res.Append(wxString::Format("%02x", in[0]));
    res.Append(wxString::Format("%02x-", in[1]));
    res.Append(wxString::Format("%02x-", in[2]));
    res.Append(wxString::Format("%02x", in[3]));
    return res;
}
wxString SmartCardReader::GetPostCode(unsigned char *in) {
    wxString res;
    res.Append(wxString::Format("%02x", in[0]));
    res.Append(wxString::Format("%02x", in[1]));
    res.Append(wxString::Format("%02x", in[2])[0]);
    return res;
}

SmartCardReader::SmartCardReader() : skipPhoto(true) {
    long retVal = SCardEstablishContext(SCARD_SCOPE_USER, 0, 0, &hSC);
    if (retVal != SCARD_S_SUCCESS) throw std::runtime_error(fmt::format("SmartCardError: {}", std::string(GetErrorMessage(retVal))));
#ifdef __LINUX__
    char rxBuffer[1024];
    unsigned long dCount = sizeof(rxBuffer) / sizeof(char);
#else
    wchar_t rxBuffer[1024];
    unsigned long dCount = sizeof(rxBuffer) / sizeof(wchar_t);
#endif
    retVal = SCardListReaders(hSC, 0, rxBuffer, &dCount);
    if (retVal != 0) throw std::runtime_error(fmt::format("SmartCardError: {}", std::string(GetErrorMessage(retVal))));
#ifdef __LINUX__
    char *p = rxBuffer;
#else
    wchar_t *p = rxBuffer;
#endif
    for (size_t i = 0; i < dCount; i++) {
        if (!*p) break;
        auto &reader = readerNames.emplace_back(p);
        p += reader.Length();
    }
}
SmartCardReader::~SmartCardReader() {
    if (hSC) SCardReleaseContext(hSC);
}

bool SmartCardReader::Connect(int readerIndex) {
    unsigned long dProtocol;
    if (readerIndex < 0 && readerIndex >= int(readerNames.size())) return false;
    long retVal = SCardConnect(hSC, readerNames[readerIndex].c_str(), SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0, &hCard, &dProtocol);
    if (retVal != 0) throw std::runtime_error(fmt::format("SmartCardError: {}", std::string(GetErrorMessage(retVal))));
    return true;
}

bool SmartCardReader::Disconnect() {
    long retVal = SCardDisconnect(hCard, SCARD_EJECT_CARD);
    if (retVal != 0) throw std::runtime_error(fmt::format("SmartCardError: {}", std::string(GetErrorMessage(retVal))));
    return true;
}

bool SmartCardReader::GetData() {
    unsigned long dLength = 256;
    unsigned char rxBuff[4096];
    unsigned long retVal = SCardTransmit(hCard, &pciT0, CmdSelectAppJPN, 15, &pciT0, rxBuff, &dLength);
    if (retVal != 0) throw std::runtime_error(fmt::format("SmartCardError:  {}", std::string(GetErrorMessage(retVal))));
    if (rxBuff[0] != 0x61 || rxBuff[1] != 0x05) throw std::runtime_error("Not MyKad");
    dLength = 256;
    retVal = SCardTransmit(hCard, &pciT0, CmdAppResponse, 5, &pciT0, rxBuff, &dLength);
    if (retVal != 0) throw std::runtime_error(fmt::format("SmartCardError: {}", std::string(GetErrorMessage(retVal))));
    unsigned char txBuff[1024];
    wxMemoryOutputStream imgStream;
    data.Clear();
    for (int fileNum = 1; fileLengths[fileNum]; fileNum++) {
        if (fileNum == 2 && skipPhoto) continue;  // skip Picture
        for (int split_offset = 0, split_length = 252; split_offset < fileLengths[fileNum]; split_offset += split_length) {
            if (split_offset + split_length > fileLengths[fileNum])
                split_length = fileLengths[fileNum] - split_offset;
            dLength = 256;
            int i = 0;
            for (i = 0; i < 8; i++)
                txBuff[i] = CmdSetLength[i];
            *(short *)(txBuff + i) = split_length;
            i += 2;
            retVal = SCardTransmit(hCard, &pciT0, txBuff, i, &pciT0, rxBuff, &dLength);
            dLength = 256;
            for (i = 0; i < 5; i++)
                txBuff[i] = CmdSelectFile[i];
            *(short *)(txBuff + i) = fileNum;
            i += 2;
            *(short *)(txBuff + i) = 1;
            i += 2;
            *(short *)(txBuff + i) = split_offset;
            i += 2;
            *(short *)(txBuff + i) = split_length;
            i += 2;
            retVal = SCardTransmit(hCard, &pciT0, txBuff, i, &pciT0, rxBuff, &dLength);

            dLength = 256;
            for (i = 0; i < 4; i++)
                txBuff[i] = CmdGetData[i];
            txBuff[i++] = (unsigned char)split_length;
            retVal = SCardTransmit(hCard, &pciT0, txBuff, i, &pciT0, rxBuff, &dLength);
            if (fileNum == 2) {
                if (split_offset == 0)
                    imgStream.Write(rxBuff + 3, dLength - 5);
                else
                    imgStream.Write(rxBuff, dLength - 2);
            } else if (fileNum == 1 && split_offset == 0) {
                data.name = Trim(rxBuff + 0x03, 0x28);
            } else if (fileNum == 1 && split_offset == 252) {
                data.ic = Trim(rxBuff + 0x111 - 252, 0x0D);
                if (rxBuff[0x11E - 252] == 'P')
                    data.sex = "Female";
                else if (rxBuff[0x11E - 252] == 'L')
                    data.sex = "Male";
                else
                    data.sex = wxChar(rxBuff[0x11E - 252]);
                data.oldic = Trim(rxBuff + 0x11F - 252, 0x08);
                data.dob = GetDate(rxBuff + 0x127 - 252);
                data.stateOfBirth = Trim(rxBuff + 0x12B - 252, 0x19);
                data.validityDate = GetDate(rxBuff + 0x144 - 252);
                data.nationality = Trim(rxBuff + 0x148 - 252, 0x12);
                data.race = Trim(rxBuff + 0x15A - 252, 0x19);
                data.religion = Trim(rxBuff + 0x173 - 252, 0x0B);
            } else if (fileNum == 4 && split_offset == 0) {
                data.postcode = GetPostCode(rxBuff + 0x5D);
                data.address.emplace_back(Trim(rxBuff + 0x79, 0x1E));
                data.address.emplace_back(data.postcode + " " + Trim(rxBuff + 0x60, 0x19));
                data.address.emplace_back(Trim(rxBuff + 0x3F, 0x1E));
                data.address.emplace_back(Trim(rxBuff + 0x21, 0x1E));
                data.address.emplace_back(Trim(rxBuff + 0x03, 0x1E));
            }
        }
        if (fileNum == 2) {
            wxMemoryInputStream inpStream(imgStream);
            face.LoadFile(inpStream, wxBITMAP_TYPE_JPEG);
        }
    }
    return true;
}

bool SmartCardReader::GetStatus(int readerIdx, double timeOut) {
    if (readerIdx < 0 && readerIdx >= int(readerNames.size())) return false;

    SCARD_READERSTATE readerState;
    memset(&readerState, 0, sizeof(SCARD_READERSTATE));
    //wxString enablePlugPlayEvent("\\\\?PnP?\\Notification");
#ifdef __LINUX__
    readerState.szReader = readerNames[readerIdx].c_str();
#else
    readerState.szReader = readerNames[readerIdx].wc_str();
#endif
    unsigned long ret = SCardGetStatusChange(hSC, timeOut, &readerState, 1);
    if (ret != SCARD_S_SUCCESS) return false;
    if (readerState.dwEventState & SCARD_STATE_PRESENT) return true;
    return false;
}

const unsigned char SmartCardReader::CmdSelectAppJPN[] = {0x00, 0xA4, 0x04, 0x00, 0x0A, 0x0A0, 0x00, 0x00, 0x00, 0x74, 0x4A, 0x50, 0x4E, 0x00, 0x10};
const unsigned char SmartCardReader::CmdAppResponse[] = {0x00, 0xC0, 0x00, 0x00, 0x05};
const unsigned char SmartCardReader::CmdSetLength[] = {0xC8, 0x32, 0x00, 0x00, 0x05, 0x08, 0x00, 0x00};  //append with ss ss
const unsigned char SmartCardReader::CmdSelectFile[] = {0xCC, 0x00, 0x00, 0x00, 0x08};                   //append with pp pp qq qq rr rr ss ss
                                                                                                         //pppp = file id,
                                                                                                         //qqqq = file group
                                                                                                         //rrrr = offset, ssss = length
const unsigned char SmartCardReader::CmdGetData[] = {0xCC, 0x06, 0x00, 0x00};                            //append with ss
const int SmartCardReader::fileLengths[] = {0, 459, 4011, 1227, 171, 43, 43, 0};
SCARD_IO_REQUEST SmartCardReader::pciT0 = {1, 8};

wxString SmartCardReader::GetErrorMessage(unsigned long errCode) {
    switch (errCode) {
        case SCARD_F_INTERNAL_ERROR: return (" An internal consistency check failed.");
        case SCARD_E_CANCELLED: return ("The action was cancelled by an SCardCancel request.");
        case SCARD_E_INVALID_HANDLE: return ("The supplied handle was invalid.");
        case SCARD_E_INVALID_PARAMETER: return ("One or more of the supplied parameters could not be properly interpreted.");
        case SCARD_E_INVALID_TARGET: return ("Registry startup information is missing or invalid.");
        case SCARD_E_NO_MEMORY: return ("Not enough memory available to complete this command.");
        case SCARD_F_WAITED_TOO_LONG: return ("An internal consistency timer has expired.");
        case SCARD_E_INSUFFICIENT_BUFFER: return ("The data buffer to receive returned data is too small for the returned data.");
        case SCARD_E_UNKNOWN_READER: return ("The specified reader name is not recognized.");
        case SCARD_E_TIMEOUT: return ("The user-specified timeout value has expired.");
        case SCARD_E_SHARING_VIOLATION: return ("The smart card cannot be accessed because of other connections outstanding.");
        case SCARD_E_NO_SMARTCARD: return ("The operation requires a smart card, but no smart card is currently in the device.");
        case SCARD_E_UNKNOWN_CARD: return ("The specified smart card name is not recognized.");
        case SCARD_E_CANT_DISPOSE: return ("The system could not dispose of the media in the requested manner.");
        case SCARD_E_PROTO_MISMATCH: return ("The requested protocols are incompatible with the protocol currently in use with the smart card.");
        case SCARD_E_NOT_READY: return ("The reader or smart card is not ready to accept commands.");
        case SCARD_E_INVALID_VALUE: return ("One or more of the supplied parameters values could not be properly interpreted.");
        case SCARD_E_SYSTEM_CANCELLED: return ("The action was cancelled by the system, presumably to log off or shut down.");
        case SCARD_F_COMM_ERROR: return ("An internal communications error has been detected.");
        case SCARD_F_UNKNOWN_ERROR: return ("An internal error has been detected, but the source is unknown.");
        case SCARD_E_INVALID_ATR: return ("An ATR obtained from the registry is not a valid ATR string.");
        case SCARD_E_NOT_TRANSACTED: return ("An attempt was made to end a non-existent transaction.");
        case SCARD_E_READER_UNAVAILABLE: return ("The specified reader is not currently available for use.");
        case SCARD_E_PCI_TOO_SMALL: return ("The PCI Receive buffer was too small.");
        case SCARD_E_READER_UNSUPPORTED: return ("The reader driver does not meet minimal requirements for support.");
        case SCARD_E_DUPLICATE_READER: return ("The reader driver did not produce a unique reader name.");
        case SCARD_E_CARD_UNSUPPORTED: return ("The smart card does not meet minimal requirements for support.");
        case SCARD_E_NO_SERVICE: return ("The Smart Card Resource Manager is not running.");
        case SCARD_E_SERVICE_STOPPED: return ("The Smart Card Resource Manager has shut down.");
        case SCARD_E_NO_READERS_AVAILABLE: return ("Cannot find a smart card reader.");
        case SCARD_W_UNSUPPORTED_CARD: return ("The reader cannot communicate with the smart card, due to ATR configuration conflicts.");
        case SCARD_W_UNRESPONSIVE_CARD: return ("The smart card is not responding to a reset.");
        case SCARD_W_UNPOWERED_CARD: return ("Power has been removed from the smart card, so that further communication is not possible.");
        case SCARD_W_RESET_CARD: return ("The smart card has been reset, so any shared state information is invalid.");
        case SCARD_W_REMOVED_CARD: return ("The smart card has been removed, so that further communication is not possible.");
#ifndef __LINUX__
        case SCARD_E_UNEXPECTED: return ("An unexpected card error has occurred.");
        case SCARD_E_ICC_INSTALLATION: return ("No Primary Provider can be found for the smart card.");
        case SCARD_E_ICC_CREATEORDER: return ("The requested order of object creation is not supported.");
        case SCARD_P_SHUTDOWN: return ("The operation has been aborted to allow the server application to exit.");
        case SCARD_E_UNSUPPORTED_FEATURE: return ("This smart card does not support the requested feature.");
        case SCARD_E_DIR_NOT_FOUND: return ("The identified directory does not exist in the smart card.");
        case SCARD_E_FILE_NOT_FOUND: return ("The identified file does not exist in the smart card.");
        case SCARD_E_NO_DIR: return ("The supplied path does not represent a smart card directory.");
        case SCARD_E_NO_FILE: return ("The supplied path does not represent a smart card file.");
        case SCARD_E_NO_ACCESS: return ("Access is denied to this file.");
        case SCARD_E_WRITE_TOO_MANY: return ("The smart card does not have enough memory to store the information.");
        case SCARD_E_BAD_SEEK: return ("There was an error trying to set the smart card file object pointer.");
        case SCARD_E_INVALID_CHV: return ("The supplied PIN is incorrect.");
        case SCARD_E_UNKNOWN_RES_MNG: return ("An unrecognized error code was returned from a layered component.");
        case SCARD_E_NO_SUCH_CERTIFICATE: return ("The requested certificate does not exist.");
        case SCARD_E_CERTIFICATE_UNAVAILABLE: return ("The requested certificate could not be obtained.");
        case SCARD_E_PIN_CACHE_EXPIRED: return ("The smart card PIN cache has expired.");
        case SCARD_E_NO_PIN_CACHE: return ("The smart card PIN cannot be cached.");
        case SCARD_E_READ_ONLY_CARD: return ("The smart card is read only and cannot be written to.");
        case SCARD_E_COMM_DATA_LOST: return ("A communications error with the smart card has been detected. Retry the operation.");
        case SCARD_E_NO_KEY_CONTAINER: return ("The requested key container does not exist on the smart card.");
        case SCARD_E_SERVER_TOO_BUSY: return ("The Smart Card Resource Manager is too busy to complete this operation.");
        case SCARD_W_SECURITY_VIOLATION: return ("Access was denied because of a security violation.");
        case SCARD_W_WRONG_CHV: return ("The card cannot be accessed because the wrong PIN was presented.");
        case SCARD_W_CHV_BLOCKED: return ("The card cannot be accessed because the maximum number of PIN entry attempts has been reached.");
        case SCARD_W_EOF: return ("The end of the smart card file has been reached.");
        case SCARD_W_CANCELLED_BY_USER: return ("The action was cancelled by the user.");
        case SCARD_W_CARD_NOT_AUTHENTICATED: return ("No PIN was presented to the smart card.");
        case SCARD_W_CACHE_ITEM_NOT_FOUND: return ("The requested item could not be found in the cache.");
        case SCARD_W_CACHE_ITEM_STALE: return ("The requested cache item is too old and was deleted from the cache.");
        case SCARD_W_CACHE_ITEM_TOO_BIG: return ("The new cache item exceeds the maximum per-item size defined for the cache.");
#endif
        default: break;
    }
    return wxString::Format("Error code unknown 0x%08X", errCode);
}

int TestCARDReader(int) {
    SmartCardReader scard;
    if (scard.GetStatus(0, INFINITE)) {
        scard.Connect(0);
        scard.skipPhoto = false;
        scard.GetData();
        scard.face.SaveFile("face.jpg");
        std::cout << "Name: " << scard.data.name << '\n';
        std::cout << "IC: " << scard.data.ic << '\n';
        std::cout << "DOB: " << scard.data.dob.FormatDate() << '\n';
        std::cout << "Address:\n";
        ;
        for (auto const &it : scard.data.address) {
            std::cout << "\t" << it << '\n';
        }
    } else
        std::cout << "\nNot ready\n";
    return 0;
}

std::vector<wxString> GetSmartCardReaders() {
    SmartCardReader scard;
    return scard.GetReaders();
}
