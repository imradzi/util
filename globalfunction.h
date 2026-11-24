#pragma once
#include <vector>
#include <memory>

class PharmacyDB;
void ExecuteProcedure(const std::string &opt);
void LoadOldData(const std::string &s);
void ImportData(const std::string &s);
bool ShuttingDown();
// Declared here (client-safe) instead of comm/eventbroadcaster.h so that
// lib_common's ControlC.cpp does not pull a server-owned header into ppClient.
// Defined per binary: appServer/GenericMain.cpp (server) and
// clientModel/GenericKedaiRuncitApp.cpp (client).
void BreakMainLoop();

#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD event);
#else
void ConsoleHandler(int);
#endif

void LoadPPOSData(const std::string &dbName);
void LoadPPOSData_SetActiveDate(PharmacyDB *db);
wxDateTime GetDate(const wxString &t);
bool GetDate(wxDateTime &dt, const wxString &t);

inline int sum(double arr[], int cnt) {
    int s = 0;
    for (int i = 0; i < cnt; i++)
        s += arr[i];
    return s;
}

