#pragma once
#include <vector>
#include <memory>

class PharmacyDB;
void ExecuteProcedure(const std::string &opt);
void LoadOldData(const std::string &s);
void ImportData(const std::string &s);
bool ShuttingDown();

#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD event);
#else
void ConsoleHandler(int);
#endif

void LoadPPOSData(const std::string &dbName);
std::wstring GetServiceName();
void LoadPPOSData_SetActiveDate(PharmacyDB *db);
wxDateTime GetDate(const wxString &t);
bool GetDate(wxDateTime &dt, const wxString &t);

inline int sum(double arr[], int cnt) {
    int s = 0;
    for (int i = 0; i < cnt; i++)
        s += arr[i];
    return s;
}

