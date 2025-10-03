#ifdef _WIN32
#include "process_utils.h"
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <codecvt>
#include <locale>

std::vector<ProcessInfo> enumerateProcesses() {
    std::vector<ProcessInfo> processes;

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return processes;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe32)) {
        do {
            ProcessInfo info;
            info.pid = pe32.th32ProcessID;
            info.exeName = pe32.szExeFile;
            processes.push_back(info);
        } while (Process32NextW(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return processes;
}

std::vector<ProcessInfo> findProcessesBySubstring(const std::wstring &substring) {
    std::vector<ProcessInfo> result;
    auto allProcesses = enumerateProcesses();

    std::wstring lowerSubstring = substring;
    std::transform(lowerSubstring.begin(), lowerSubstring.end(),
                   lowerSubstring.begin(), ::towlower);

    for (const auto &proc : allProcesses) {
        std::wstring lowerName = proc.exeName;
        std::transform(lowerName.begin(), lowerName.end(),
                       lowerName.begin(), ::towlower);

        if (lowerName.find(lowerSubstring) != std::wstring::npos) {
            result.push_back(proc);
        }
    }

    return result;
}

HANDLE openProcessBasic(DWORD pid, bool requireWrite) {
    DWORD access = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
    if (requireWrite) {
        access |= PROCESS_VM_WRITE | PROCESS_VM_OPERATION;
    }

    return OpenProcess(access, FALSE, pid);
}

std::string wideToUtf8(const std::wstring &w) {
    if (w.empty()) return {};

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.length(),
                                          nullptr, 0, nullptr, nullptr);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.length(),
                        &result[0], size_needed, nullptr, nullptr);
    return result;
}

#endif

