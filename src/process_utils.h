#pragma once
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>

struct ProcessInfo {
    DWORD pid{};
    std::wstring exeName; // executable name
};

// Enumerate all running processes.
std::vector<ProcessInfo> enumerateProcesses();

// Find processes whose (case-insensitive) executable name contains the substring.
std::vector<ProcessInfo> findProcessesBySubstring(const std::wstring &substring);

// Open a process handle with read + query rights (optionally write if later extended).
HANDLE openProcessBasic(DWORD pid, bool requireWrite = false);

// Simple helper to convert wide string to UTF-8 (for console output).
std::string wideToUtf8(const std::wstring &w);
#else
// Stubs for non-Windows builds.
#include <string>
#include <vector>
struct ProcessInfo { int pid; std::string exeName; };
inline std::vector<ProcessInfo> enumerateProcesses() { return {}; }
inline std::vector<ProcessInfo> findProcessesBySubstring(const std::string&) { return {}; }
inline void* openProcessBasic(int, bool=false) { return nullptr; }
inline std::string wideToUtf8(const std::wstring &) { return {}; }
#endif

