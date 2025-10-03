#ifdef _WIN32
#define UNICODE
#define _UNICODE
#include "process_utils.h"
#include "memory_scanner.h"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "comctl32.lib")

// Control IDs
#define IDC_PROCESS_LIST 1001
#define IDC_BTN_ATTACH 1002
#define IDC_VALUE_INPUT 1003
#define IDC_BTN_FIRST_SCAN 1004
#define IDC_BTN_NEXT_SCAN 1005
#define IDC_BTN_CHANGED 1006
#define IDC_BTN_UNCHANGED 1007
#define IDC_RESULT_LIST 1008
#define IDC_STATUS_BAR 1009
#define IDC_ADDRESS_INPUT 1010
#define IDC_NEW_VALUE_INPUT 1011
#define IDC_BTN_WRITE 1012
#define IDC_BTN_READ 1013
#define IDC_BTN_RESET 1014
#define IDC_COMBO_TYPE 1015
#define IDC_BTN_REFRESH 1016
#define IDC_PROCESS_LABEL 1017

// Scan value types
enum class ScanValueType {
    INT32,
    INT64,
    FLOAT,
    DOUBLE,
    STRING_ASCII,
    STRING_UNICODE
};

// Global variables
HWND g_hMainWindow = nullptr;
HWND g_hProcessList = nullptr;
HWND g_hResultList = nullptr;
HWND g_hStatusBar = nullptr;
HWND g_hValueInput = nullptr;
HWND g_hAddressInput = nullptr;
HWND g_hNewValueInput = nullptr;
HWND g_hTypeCombo = nullptr;
HWND g_hProcessLabel = nullptr;

HANDLE g_hProcess = nullptr;
MemoryScanner* g_pScanner = nullptr;
std::vector<MemoryMatch<int32_t>> g_currentMatches;
std::vector<MemoryMatch<std::string>> g_stringMatches;
std::vector<MemoryMatch<std::wstring>> g_wstringMatches;
std::vector<std::wstring> g_displayedAddresses;
std::vector<std::wstring> g_displayedValues;
std::wstring g_currentProcessName = L"";
ScanValueType g_currentScanType = ScanValueType::INT32;
bool g_hasInitialScan = false;

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void PopulateProcessList();
void AttachToProcess();
void PerformFirstScan();
void PerformNextScan();
void PerformChangedScan();
void PerformUnchangedScan();
void UpdateResultList();
void WriteValue();
void ReadValue();
void ResetScan();
void UpdateStatusBar(const std::wstring& text);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    // Register window class
    const wchar_t CLASS_NAME[] = L"MemoryScannerWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassW(&wc);

    // Create window
    g_hMainWindow = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Memory Scanner",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (g_hMainWindow == nullptr) {
        return 0;
    }

    ShowWindow(g_hMainWindow, nCmdShow);
    UpdateWindow(g_hMainWindow);

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    if (g_pScanner) delete g_pScanner;
    if (g_hProcess) CloseHandle(g_hProcess);

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateControls(hwnd);
            PopulateProcessList();
            UpdateStatusBar(L"Bereit");
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BTN_ATTACH:
                    AttachToProcess();
                    break;
                case IDC_BTN_FIRST_SCAN:
                    PerformFirstScan();
                    break;
                case IDC_BTN_NEXT_SCAN:
                    PerformNextScan();
                    break;
                case IDC_BTN_CHANGED:
                    PerformChangedScan();
                    break;
                case IDC_BTN_UNCHANGED:
                    PerformUnchangedScan();
                    break;
                case IDC_BTN_WRITE:
                    WriteValue();
                    break;
                case IDC_BTN_READ:
                    ReadValue();
                    break;
                case IDC_BTN_RESET:
                    ResetScan();
                    break;
                case IDC_BTN_REFRESH:
                    PopulateProcessList();
                    break;
            }
            return 0;

        case WM_SIZE:
            if (g_hStatusBar) {
                SendMessage(g_hStatusBar, WM_SIZE, 0, 0);
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CreateControls(HWND hwnd) {
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);

    // Process selection group
    CreateWindowW(L"BUTTON", L"Prozess Auswahl",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 10, 300, 200, hwnd, nullptr, hInstance, nullptr);

    g_hProcessList = CreateWindowW(WC_LISTBOXW, nullptr,
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
        20, 30, 280, 140, hwnd, (HMENU)IDC_PROCESS_LIST, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"An Prozess anhängen",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 175, 140, 25, hwnd, (HMENU)IDC_BTN_ATTACH, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Aktualisieren",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        165, 175, 135, 25, hwnd, (HMENU)IDC_BTN_REFRESH, hInstance, nullptr);

    // Scan controls group
    CreateWindowW(L"BUTTON", L"Scan Optionen",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        320, 10, 300, 200, hwnd, nullptr, hInstance, nullptr);

    CreateWindowW(L"STATIC", L"Typ:",
        WS_CHILD | WS_VISIBLE,
        330, 35, 50, 20, hwnd, nullptr, hInstance, nullptr);

    g_hTypeCombo = CreateWindowW(WC_COMBOBOXW, nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        385, 32, 220, 200, hwnd, (HMENU)IDC_COMBO_TYPE, hInstance, nullptr);

    // Add scan type options
    SendMessageW(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"4 Byte (int32)");
    SendMessageW(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"8 Byte (int64)");
    SendMessageW(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"Float");
    SendMessageW(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"Double");
    SendMessageW(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"String (ASCII)");
    SendMessageW(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"String (Unicode)");
    SendMessageW(g_hTypeCombo, CB_SETCURSEL, 0, 0); // Default to INT32

    CreateWindowW(L"STATIC", L"Wert:",
        WS_CHILD | WS_VISIBLE,
        330, 70, 50, 20, hwnd, nullptr, hInstance, nullptr);

    g_hValueInput = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        385, 67, 220, 25, hwnd, (HMENU)IDC_VALUE_INPUT, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Erster Scan",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        330, 100, 135, 30, hwnd, (HMENU)IDC_BTN_FIRST_SCAN, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Nächster Scan",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        470, 100, 135, 30, hwnd, (HMENU)IDC_BTN_NEXT_SCAN, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Geändert",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        330, 135, 135, 30, hwnd, (HMENU)IDC_BTN_CHANGED, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Ungeändert",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        470, 135, 135, 30, hwnd, (HMENU)IDC_BTN_UNCHANGED, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Scan zurücksetzen",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        330, 170, 275, 30, hwnd, (HMENU)IDC_BTN_RESET, hInstance, nullptr);

    // Memory modification group
    CreateWindowW(L"BUTTON", L"Speicher bearbeiten",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        630, 10, 340, 200, hwnd, nullptr, hInstance, nullptr);

    CreateWindowW(L"STATIC", L"Adresse (hex):",
        WS_CHILD | WS_VISIBLE,
        640, 35, 100, 20, hwnd, nullptr, hInstance, nullptr);

    g_hAddressInput = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        745, 32, 215, 25, hwnd, (HMENU)IDC_ADDRESS_INPUT, hInstance, nullptr);

    CreateWindowW(L"STATIC", L"Neuer Wert:",
        WS_CHILD | WS_VISIBLE,
        640, 70, 100, 20, hwnd, nullptr, hInstance, nullptr);

    g_hNewValueInput = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        745, 67, 215, 25, hwnd, (HMENU)IDC_NEW_VALUE_INPUT, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Wert schreiben",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        640, 100, 155, 30, hwnd, (HMENU)IDC_BTN_WRITE, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Wert lesen",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        805, 100, 155, 30, hwnd, (HMENU)IDC_BTN_READ, hInstance, nullptr);

    // Result list
    CreateWindowW(L"STATIC", L"Gefundene Adressen:",
        WS_CHILD | WS_VISIBLE,
        10, 220, 150, 20, hwnd, nullptr, hInstance, nullptr);

    g_hResultList = CreateWindowExW(0, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_EDITLABELS,
        10, 245, 960, 360, hwnd, (HMENU)IDC_RESULT_LIST, hInstance, nullptr);

    // Set up list view columns
    LVCOLUMNW lvc = {};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;

    lvc.cx = 150;
    lvc.pszText = (LPWSTR)L"Adresse";
    ListView_InsertColumn(g_hResultList, 0, &lvc);

    lvc.cx = 150;
    lvc.pszText = (LPWSTR)L"Wert";
    ListView_InsertColumn(g_hResultList, 1, &lvc);

    // Status bar
    g_hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hwnd, (HMENU)IDC_STATUS_BAR, hInstance, nullptr);

    // Current process label
    g_hProcessLabel = CreateWindowW(L"STATIC", L"Ausgewählter Prozess: ",
        WS_CHILD | WS_VISIBLE,
        10, 620, 300, 25, hwnd, nullptr, hInstance, nullptr);
}

void PopulateProcessList() {
    SendMessage(g_hProcessList, LB_RESETCONTENT, 0, 0);

    auto processes = enumerateProcesses();
    for (const auto& proc : processes) {
        std::wstringstream ss;
        ss << L"[" << proc.pid << L"] " << proc.exeName;
        SendMessageW(g_hProcessList, LB_ADDSTRING, 0, (LPARAM)ss.str().c_str());
    }

    UpdateStatusBar(L"Prozessliste aktualisiert - " + std::to_wstring(processes.size()) + L" Prozesse gefunden");
}

void AttachToProcess() {
    int index = (int)SendMessage(g_hProcessList, LB_GETCURSEL, 0, 0);
    if (index == LB_ERR) {
        MessageBoxW(g_hMainWindow, L"Bitte wählen Sie einen Prozess aus!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t buffer[256];
    SendMessageW(g_hProcessList, LB_GETTEXT, index, (LPARAM)buffer);

    // Extract PID from string [PID] Name
    std::wstring text(buffer);
    size_t start = text.find(L"[") + 1;
    size_t end = text.find(L"]");
    DWORD pid = std::stoi(text.substr(start, end - start));

    // Clean up old scanner
    if (g_pScanner) {
        delete g_pScanner;
        g_pScanner = nullptr;
    }
    if (g_hProcess) {
        CloseHandle(g_hProcess);
        g_hProcess = nullptr;
    }

    g_hProcess = openProcessBasic(pid, true);
    if (g_hProcess == nullptr) {
        MessageBoxW(g_hMainWindow,
            L"Fehler beim Öffnen des Prozesses!\n\nStellen Sie sicher, dass Sie:\n- Administrator-Rechte haben\n- Der Prozess noch läuft",
            L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    g_pScanner = new MemoryScanner(g_hProcess);
    g_hasInitialScan = false;
    g_currentMatches.clear();
    ListView_DeleteAllItems(g_hResultList);

    // Update process label
    SetWindowTextW(g_hProcessLabel, (L"Ausgewählter Prozess: " + text).c_str());

    UpdateStatusBar(L"✓ An Prozess angehängt (PID: " + std::to_wstring(pid) + L")");
}

void PerformFirstScan() {
    if (!g_pScanner) {
        MessageBoxW(g_hMainWindow, L"Bitte hängen Sie sich zuerst an einen Prozess an!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t buffer[256];
    GetWindowTextW(g_hValueInput, buffer, 256);

    // Get selected scan type
    int typeIndex = (int)SendMessage(g_hTypeCombo, CB_GETCURSEL, 0, 0);
    g_currentScanType = static_cast<ScanValueType>(typeIndex);

    // Check if input is empty
    bool isEmptyInput = (wcslen(buffer) == 0);

    UpdateStatusBar(L"Scanne Speicher... Bitte warten...");
    UpdateWindow(g_hMainWindow);

    // Clear all match vectors
    g_currentMatches.clear();
    g_stringMatches.clear();
    g_wstringMatches.clear();

    switch (g_currentScanType) {
        case ScanValueType::INT32:
            if (isEmptyInput) {
                g_currentMatches = g_pScanner->scanAllValues<int32_t>();
            } else {
                int32_t value = _wtoi(buffer);
                g_currentMatches = g_pScanner->scanForValue(value);
            }
            break;

        case ScanValueType::INT64:
            if (isEmptyInput) {
                // For int64, we need to store in currentMatches as int32 for display purposes
                // or create a separate int64 vector
                auto matches = g_pScanner->scanAllValues<int64_t>();
                for (const auto& m : matches) {
                    MemoryMatch<int32_t> converted;
                    converted.address = m.address;
                    converted.value = static_cast<int32_t>(m.value);
                    g_currentMatches.push_back(converted);
                }
            } else {
                int64_t value = _wtoi64(buffer);
                auto matches = g_pScanner->scanForValue(value);
                for (const auto& m : matches) {
                    MemoryMatch<int32_t> converted;
                    converted.address = m.address;
                    converted.value = static_cast<int32_t>(m.value);
                    g_currentMatches.push_back(converted);
                }
            }
            break;

        case ScanValueType::FLOAT:
            if (!isEmptyInput) {
                float value = std::stof(buffer);
                auto matches = g_pScanner->scanForValue(value);
                for (const auto& m : matches) {
                    MemoryMatch<int32_t> converted;
                    converted.address = m.address;
                    converted.value = static_cast<int32_t>(m.value);
                    g_currentMatches.push_back(converted);
                }
            }
            break;

        case ScanValueType::DOUBLE:
            if (!isEmptyInput) {
                double value = std::stod(buffer);
                auto matches = g_pScanner->scanForValue(value);
                for (const auto& m : matches) {
                    MemoryMatch<int32_t> converted;
                    converted.address = m.address;
                    converted.value = static_cast<int32_t>(m.value);
                    g_currentMatches.push_back(converted);
                }
            }
            break;

        case ScanValueType::STRING_ASCII:
            if (!isEmptyInput) {
                // Convert wchar_t to ASCII string
                char asciiBuffer[256];
                wcstombs(asciiBuffer, buffer, 256);
                std::string searchStr(asciiBuffer);
                g_stringMatches = g_pScanner->scanForString(searchStr);
            }
            break;

        case ScanValueType::STRING_UNICODE:
            if (!isEmptyInput) {
                std::wstring searchStr(buffer);
                g_wstringMatches = g_pScanner->scanForWideString(searchStr);
            }
            break;
    }

    g_hasInitialScan = true;

    UpdateResultList();

    // Show status based on type
    size_t totalFound = g_currentMatches.size() + g_stringMatches.size() + g_wstringMatches.size();
    std::wstringstream status;
    status << L"✓ Scan abgeschlossen! Gefunden: " << totalFound << L" Adressen";
    UpdateStatusBar(status.str());
}

void PerformNextScan() {
    if (!g_hasInitialScan) {
        MessageBoxW(g_hMainWindow, L"Führen Sie zuerst einen ersten Scan durch!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    // Check if we have any matches
    bool hasMatches = !g_currentMatches.empty() || !g_stringMatches.empty() || !g_wstringMatches.empty();
    if (!hasMatches) {
        MessageBoxW(g_hMainWindow, L"Keine Ergebnisse zum Filtern vorhanden!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t buffer[256];
    GetWindowTextW(g_hValueInput, buffer, 256);

    UpdateStatusBar(L"Filtere Ergebnisse...");
    UpdateWindow(g_hMainWindow);

    // Filter based on current scan type
    switch (g_currentScanType) {
        case ScanValueType::INT32: {
            int32_t value = _wtoi(buffer);
            // Use the proper filter that updates the values
            std::vector<MemoryMatch<int32_t>> newMatches;
            for (const auto& match : g_currentMatches) {
                int32_t currentValue;
                if (g_pScanner->readValue(match.address, currentValue) && currentValue == value) {
                    MemoryMatch<int32_t> newMatch;
                    newMatch.address = match.address;
                    newMatch.value = currentValue; // Store the CURRENT value
                    newMatches.push_back(newMatch);
                }
            }
            g_currentMatches = newMatches;
            break;
        }
        case ScanValueType::INT64: {
            int64_t value = _wtoi64(buffer);
            std::vector<MemoryMatch<int32_t>> newMatches;
            for (const auto& match : g_currentMatches) {
                int64_t currentValue;
                if (g_pScanner->readValue(match.address, currentValue) && currentValue == value) {
                    MemoryMatch<int32_t> newMatch;
                    newMatch.address = match.address;
                    newMatch.value = static_cast<int32_t>(currentValue);
                    newMatches.push_back(newMatch);
                }
            }
            g_currentMatches = newMatches;
            break;
        }
        case ScanValueType::FLOAT: {
            float value = std::stof(buffer);
            std::vector<MemoryMatch<int32_t>> newMatches;
            for (const auto& match : g_currentMatches) {
                float currentValue;
                if (g_pScanner->readValue(match.address, currentValue) && currentValue == value) {
                    MemoryMatch<int32_t> newMatch;
                    newMatch.address = match.address;
                    newMatch.value = static_cast<int32_t>(currentValue);
                    newMatches.push_back(newMatch);
                }
            }
            g_currentMatches = newMatches;
            break;
        }
        case ScanValueType::DOUBLE: {
            double value = std::stod(buffer);
            std::vector<MemoryMatch<int32_t>> newMatches;
            for (const auto& match : g_currentMatches) {
                double currentValue;
                if (g_pScanner->readValue(match.address, currentValue) && currentValue == value) {
                    MemoryMatch<int32_t> newMatch;
                    newMatch.address = match.address;
                    newMatch.value = static_cast<int32_t>(currentValue);
                    newMatches.push_back(newMatch);
                }
            }
            g_currentMatches = newMatches;
            break;
        }
        case ScanValueType::STRING_ASCII: {
            char asciiBuffer[256];
            wcstombs(asciiBuffer, buffer, 256);
            std::string searchStr(asciiBuffer);

            // Filter string matches manually
            std::vector<MemoryMatch<std::string>> newMatches;
            for (const auto& match : g_stringMatches) {
                std::string currentValue;
                currentValue.resize(searchStr.length());
                if (g_pScanner->readMemory(match.address, &currentValue[0], searchStr.length())) {
                    if (currentValue == searchStr) {
                        MemoryMatch<std::string> newMatch;
                        newMatch.address = match.address;
                        newMatch.value = currentValue; // Store current value
                        newMatches.push_back(newMatch);
                    }
                }
            }
            g_stringMatches = newMatches;
            break;
        }
        case ScanValueType::STRING_UNICODE: {
            std::wstring searchStr(buffer);

            // Filter wstring matches manually
            std::vector<MemoryMatch<std::wstring>> newMatches;
            for (const auto& match : g_wstringMatches) {
                std::wstring currentValue;
                currentValue.resize(searchStr.length());
                if (g_pScanner->readMemory(match.address, &currentValue[0], searchStr.length() * sizeof(wchar_t))) {
                    if (currentValue == searchStr) {
                        MemoryMatch<std::wstring> newMatch;
                        newMatch.address = match.address;
                        newMatch.value = currentValue; // Store current value
                        newMatches.push_back(newMatch);
                    }
                }
            }
            g_wstringMatches = newMatches;
            break;
        }
    }

    UpdateResultList();

    size_t totalFound = g_currentMatches.size() + g_stringMatches.size() + g_wstringMatches.size();
    UpdateStatusBar(L"✓ Scan abgeschlossen! Verbleibend: " + std::to_wstring(totalFound) + L" Adressen");
}

void PerformChangedScan() {
    if (!g_hasInitialScan) {
        MessageBoxW(g_hMainWindow, L"Führen Sie zuerst einen ersten Scan durch!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    // Check if we have any matches
    bool hasMatches = !g_currentMatches.empty() || !g_stringMatches.empty() || !g_wstringMatches.empty();
    if (!hasMatches) {
        MessageBoxW(g_hMainWindow, L"Keine Ergebnisse zum Filtern vorhanden!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    UpdateStatusBar(L"Scanne nach geänderten Werten...");
    UpdateWindow(g_hMainWindow);

    // Filter based on current scan type
    switch (g_currentScanType) {
        case ScanValueType::INT32:
        case ScanValueType::INT64:
        case ScanValueType::FLOAT:
        case ScanValueType::DOUBLE:
            g_currentMatches = g_pScanner->filterByChanged(g_currentMatches);
            break;

        case ScanValueType::STRING_ASCII: {
            // Filter string matches for changed values
            std::vector<MemoryMatch<std::string>> newMatches;
            for (const auto& match : g_stringMatches) {
                std::string currentValue;
                currentValue.resize(match.value.length());
                if (g_pScanner->readMemory(match.address, &currentValue[0], match.value.length())) {
                    if (currentValue != match.value) {
                        MemoryMatch<std::string> newMatch;
                        newMatch.address = match.address;
                        newMatch.value = currentValue;
                        newMatches.push_back(newMatch);
                    }
                }
            }
            g_stringMatches = newMatches;
            break;
        }

        case ScanValueType::STRING_UNICODE: {
            // Filter wstring matches for changed values
            std::vector<MemoryMatch<std::wstring>> newMatches;
            for (const auto& match : g_wstringMatches) {
                std::wstring currentValue;
                currentValue.resize(match.value.length());
                if (g_pScanner->readMemory(match.address, &currentValue[0], match.value.length() * sizeof(wchar_t))) {
                    if (currentValue != match.value) {
                        MemoryMatch<std::wstring> newMatch;
                        newMatch.address = match.address;
                        newMatch.value = currentValue;
                        newMatches.push_back(newMatch);
                    }
                }
            }
            g_wstringMatches = newMatches;
            break;
        }
    }

    UpdateResultList();

    size_t totalFound = g_currentMatches.size() + g_stringMatches.size() + g_wstringMatches.size();
    UpdateStatusBar(L"✓ Scan abgeschlossen! Verbleibend: " + std::to_wstring(totalFound) + L" Adressen");
}

void PerformUnchangedScan() {
    if (!g_hasInitialScan) {
        MessageBoxW(g_hMainWindow, L"Führen Sie zuerst einen ersten Scan durch!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    // Check if we have any matches
    bool hasMatches = !g_currentMatches.empty() || !g_stringMatches.empty() || !g_wstringMatches.empty();
    if (!hasMatches) {
        MessageBoxW(g_hMainWindow, L"Keine Ergebnisse zum Filtern vorhanden!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    UpdateStatusBar(L"Scanne nach ungeänderten Werten...");
    UpdateWindow(g_hMainWindow);

    // Filter based on current scan type
    switch (g_currentScanType) {
        case ScanValueType::INT32:
        case ScanValueType::INT64:
        case ScanValueType::FLOAT:
        case ScanValueType::DOUBLE:
            g_currentMatches = g_pScanner->filterByUnchanged(g_currentMatches);
            break;

        case ScanValueType::STRING_ASCII: {
            // Filter string matches for unchanged values
            std::vector<MemoryMatch<std::string>> newMatches;
            for (const auto& match : g_stringMatches) {
                std::string currentValue;
                currentValue.resize(match.value.length());
                if (g_pScanner->readMemory(match.address, &currentValue[0], match.value.length())) {
                    if (currentValue == match.value) {
                        newMatches.push_back(match);
                    }
                }
            }
            g_stringMatches = newMatches;
            break;
        }

        case ScanValueType::STRING_UNICODE: {
            // Filter wstring matches for unchanged values
            std::vector<MemoryMatch<std::wstring>> newMatches;
            for (const auto& match : g_wstringMatches) {
                std::wstring currentValue;
                currentValue.resize(match.value.length());
                if (g_pScanner->readMemory(match.address, &currentValue[0], match.value.length() * sizeof(wchar_t))) {
                    if (currentValue == match.value) {
                        newMatches.push_back(match);
                    }
                }
            }
            g_wstringMatches = newMatches;
            break;
        }
    }

    UpdateResultList();

    size_t totalFound = g_currentMatches.size() + g_stringMatches.size() + g_wstringMatches.size();
    UpdateStatusBar(L"✓ Scan abgeschlossen! Verbleibend: " + std::to_wstring(totalFound) + L" Adressen");
}

void UpdateResultList() {
    ListView_DeleteAllItems(g_hResultList);

    // Clear old display strings
    g_displayedAddresses.clear();
    g_displayedValues.clear();

    // Calculate total matches across all types
    size_t totalMatches = g_currentMatches.size() + g_stringMatches.size() + g_wstringMatches.size();
    size_t maxDisplay = std::min<size_t>(totalMatches, 1000);

    // Reserve space for better performance
    g_displayedAddresses.reserve(maxDisplay);
    g_displayedValues.reserve(maxDisplay);

    // Add int32 matches
    for (size_t i = 0; i < g_currentMatches.size() && g_displayedAddresses.size() < maxDisplay; i++) {
        std::wstringstream ss;
        ss << L"0x" << std::hex << std::setw(16) << std::setfill(L'0') << g_currentMatches[i].address;
        g_displayedAddresses.push_back(ss.str());
        g_displayedValues.push_back(std::to_wstring(g_currentMatches[i].value));
    }

    // Add ASCII string matches
    for (size_t i = 0; i < g_stringMatches.size() && g_displayedAddresses.size() < maxDisplay; i++) {
        std::wstringstream ss;
        ss << L"0x" << std::hex << std::setw(16) << std::setfill(L'0') << g_stringMatches[i].address;
        g_displayedAddresses.push_back(ss.str());

        // Convert ASCII string to wstring for display
        std::wstring wstr(g_stringMatches[i].value.begin(), g_stringMatches[i].value.end());
        g_displayedValues.push_back(wstr);
    }

    // Add Unicode string matches
    for (size_t i = 0; i < g_wstringMatches.size() && g_displayedAddresses.size() < maxDisplay; i++) {
        std::wstringstream ss;
        ss << L"0x" << std::hex << std::setw(16) << std::setfill(L'0') << g_wstringMatches[i].address;
        g_displayedAddresses.push_back(ss.str());
        g_displayedValues.push_back(g_wstringMatches[i].value);
    }

    // Display in ListView
    for (size_t i = 0; i < g_displayedAddresses.size(); i++) {
        LVITEMW lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)i;
        lvi.pszText = const_cast<LPWSTR>(g_displayedAddresses[i].c_str());
        ListView_InsertItem(g_hResultList, &lvi);

        ListView_SetItemText(g_hResultList, (int)i, 1, const_cast<LPWSTR>(g_displayedValues[i].c_str()));
    }

    if (totalMatches > maxDisplay) {
        UpdateStatusBar(L"Zeige " + std::to_wstring(maxDisplay) + L" von " +
                       std::to_wstring(totalMatches) + L" Treffern");
    }
}

void WriteValue() {
    if (!g_pScanner) {
        MessageBoxW(g_hMainWindow, L"Bitte hängen Sie sich zuerst an einen Prozess an!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t addrBuffer[32];
    wchar_t valueBuffer[256];

    GetWindowTextW(g_hAddressInput, addrBuffer, 32);
    GetWindowTextW(g_hNewValueInput, valueBuffer, 256);

    if (wcslen(addrBuffer) == 0 || wcslen(valueBuffer) == 0) {
        MessageBoxW(g_hMainWindow, L"Bitte geben Sie Adresse und Wert ein!", L"Fehler", MB_OK | MB_ICONWARNING);
        return;
    }

    uintptr_t address = std::stoull(addrBuffer, nullptr, 16);
    bool success = false;

    // Write based on current scan type
    switch (g_currentScanType) {
        case ScanValueType::INT32: {
            int32_t value = _wtoi(valueBuffer);
            success = g_pScanner->writeValue(address, value);
            break;
        }
        case ScanValueType::INT64: {
            int64_t value = _wtoi64(valueBuffer);
            success = g_pScanner->writeValue(address, value);
            break;
        }
        case ScanValueType::FLOAT: {
            float value = std::stof(valueBuffer);
            success = g_pScanner->writeValue(address, value);
            break;
        }
        case ScanValueType::DOUBLE: {
            double value = std::stod(valueBuffer);
            success = g_pScanner->writeValue(address, value);
            break;
        }
        case ScanValueType::STRING_ASCII: {
            // Convert wchar_t to ASCII string
            char asciiBuffer[256];
            wcstombs(asciiBuffer, valueBuffer, 256);
            std::string str(asciiBuffer);
            success = g_pScanner->writeMemory(address, str.c_str(), str.length());
            break;
        }
        case ScanValueType::STRING_UNICODE: {
            std::wstring str(valueBuffer);
            success = g_pScanner->writeMemory(address, str.c_str(), str.length() * sizeof(wchar_t));
            break;
        }
    }

    if (success) {
        UpdateStatusBar(L"✓ Wert erfolgreich geschrieben!");
        MessageBoxW(g_hMainWindow, L"Wert erfolgreich geschrieben!", L"Erfolg", MB_OK | MB_ICONINFORMATION);
    } else {
        UpdateStatusBar(L"✗ Fehler beim Schreiben des Werts");
        MessageBoxW(g_hMainWindow, L"Fehler beim Schreiben des Werts!", L"Fehler", MB_OK | MB_ICONERROR);
    }
}

void ReadValue() {
    if (!g_pScanner) {
        MessageBoxW(g_hMainWindow, L"Bitte hängen Sie sich zuerst an einen Prozess an!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t addrBuffer[32];
    GetWindowTextW(g_hAddressInput, addrBuffer, 32);

    if (wcslen(addrBuffer) == 0) {
        MessageBoxW(g_hMainWindow, L"Bitte geben Sie eine Adresse ein!", L"Fehler", MB_OK | MB_ICONWARNING);
        return;
    }

    uintptr_t address = std::stoull(addrBuffer, nullptr, 16);
    std::wstringstream ss;
    ss << L"Wert an Adresse 0x" << std::hex << address << L":\n\n";
    bool success = false;

    // Read based on current scan type
    switch (g_currentScanType) {
        case ScanValueType::INT32: {
            int32_t value;
            if (g_pScanner->readValue(address, value)) {
                ss << std::dec << value;
                SetWindowTextW(g_hNewValueInput, std::to_wstring(value).c_str());
                success = true;
            }
            break;
        }
        case ScanValueType::INT64: {
            int64_t value;
            if (g_pScanner->readValue(address, value)) {
                ss << std::dec << value;
                SetWindowTextW(g_hNewValueInput, std::to_wstring(value).c_str());
                success = true;
            }
            break;
        }
        case ScanValueType::FLOAT: {
            float value;
            if (g_pScanner->readValue(address, value)) {
                ss << value;
                SetWindowTextW(g_hNewValueInput, std::to_wstring(value).c_str());
                success = true;
            }
            break;
        }
        case ScanValueType::DOUBLE: {
            double value;
            if (g_pScanner->readValue(address, value)) {
                ss << value;
                SetWindowTextW(g_hNewValueInput, std::to_wstring(value).c_str());
                success = true;
            }
            break;
        }
        case ScanValueType::STRING_ASCII: {
            // Read up to 256 bytes as ASCII string
            char buffer[256] = {0};
            if (g_pScanner->readMemory(address, buffer, 255)) {
                std::string str(buffer);
                std::wstring wstr(str.begin(), str.end());
                ss << wstr;
                SetWindowTextW(g_hNewValueInput, wstr.c_str());
                success = true;
            }
            break;
        }
        case ScanValueType::STRING_UNICODE: {
            // Read up to 128 wchars as Unicode string
            wchar_t buffer[128] = {0};
            if (g_pScanner->readMemory(address, buffer, 127 * sizeof(wchar_t))) {
                std::wstring str(buffer);
                ss << str;
                SetWindowTextW(g_hNewValueInput, str.c_str());
                success = true;
            }
            break;
        }
    }

    if (success) {
        MessageBoxW(g_hMainWindow, ss.str().c_str(), L"Wert gelesen", MB_OK | MB_ICONINFORMATION);
        UpdateStatusBar(L"✓ Wert erfolgreich gelesen");
    } else {
        MessageBoxW(g_hMainWindow, L"Fehler beim Lesen des Werts!", L"Fehler", MB_OK | MB_ICONERROR);
        UpdateStatusBar(L"✗ Fehler beim Lesen des Werts");
    }
}

void ResetScan() {
    g_currentMatches.clear();
    g_stringMatches.clear();
    g_wstringMatches.clear();
    g_hasInitialScan = false;
    ListView_DeleteAllItems(g_hResultList);
    UpdateStatusBar(L"✓ Scan zurückgesetzt");
}

void UpdateStatusBar(const std::wstring& text) {
    if (g_hStatusBar) {
        SendMessageW(g_hStatusBar, SB_SETTEXTW, 0, (LPARAM)text.c_str());
    }
}

#else
#include <iostream>
int main() {
    std::cout << "Dieses Programm funktioniert nur unter Windows!\n";
    return 1;
}
#endif
