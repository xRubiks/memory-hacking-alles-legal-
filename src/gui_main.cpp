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
void UpdateInputLimitForAddress(uintptr_t address);
void OnAddressInputChanged();
void OnResultListItemSelected();

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
        L"Memory Scanner - Professional",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 850,
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
            UpdateStatusBar(L"Bereit - Memory Scanner Professional");
            return 0;

        case WM_KEYDOWN:
            // Handle keyboard shortcuts
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                switch (wParam) {
                    case 'A': // STRG+A - Select All in focused edit control
                        {
                            HWND focused = GetFocus();
                            if (focused == g_hValueInput || focused == g_hNewValueInput || focused == g_hAddressInput) {
                                SendMessage(focused, EM_SETSEL, 0, -1);
                            }
                        }
                        return 0;
                    case 'C': // STRG+C - Copy selection
                        {
                            HWND focused = GetFocus();
                            if (focused == g_hValueInput || focused == g_hNewValueInput || focused == g_hAddressInput) {
                                SendMessage(focused, WM_COPY, 0, 0);
                            }
                        }
                        return 0;
                }
            }
            break;

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
                case IDC_ADDRESS_INPUT:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        OnAddressInputChanged();
                    }
                    break;
                case IDC_COMBO_TYPE:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        int typeIndex = (int)SendMessage(g_hTypeCombo, CB_GETCURSEL, 0, 0);
                        g_currentScanType = static_cast<ScanValueType>(typeIndex);
                        OnAddressInputChanged();
                    }
                    break;
            }
            return 0;

        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            if (nmhdr->idFrom == IDC_RESULT_LIST && nmhdr->code == LVN_ITEMCHANGED) {
                LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                if (pnmv->uNewState & LVIS_SELECTED) {
                    OnResultListItemSelected();
                }
            }
            return 0;
        }

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

    // =================================================================================
    // TOP ROW: Process Selection & Scan Options & Memory Editor
    // =================================================================================

    // Process Selection Group (Links)
    CreateWindowW(L"BUTTON", L"🔍 Prozess Auswahl",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        15, 15, 320, 220, hwnd, nullptr, hInstance, nullptr);

    g_hProcessList = CreateWindowW(WC_LISTBOXW, nullptr,
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
        25, 40, 300, 150, hwnd, (HMENU)IDC_PROCESS_LIST, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"📎 An Prozess anhängen",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        25, 200, 145, 30, hwnd, (HMENU)IDC_BTN_ATTACH, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"🔄 Aktualisieren",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        180, 200, 145, 30, hwnd, (HMENU)IDC_BTN_REFRESH, hInstance, nullptr);

    // Scan Options Group (Mitte)
    CreateWindowW(L"BUTTON", L"🔬 Scan Optionen",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        350, 15, 400, 220, hwnd, nullptr, hInstance, nullptr);

    CreateWindowW(L"STATIC", L"Datentyp:",
        WS_CHILD | WS_VISIBLE,
        360, 40, 80, 20, hwnd, nullptr, hInstance, nullptr);

    g_hTypeCombo = CreateWindowW(WC_COMBOBOXW, nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        450, 37, 280, 200, hwnd, (HMENU)IDC_COMBO_TYPE, hInstance, nullptr);

    // Add scan type options
    SendMessageW(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"4 Byte (int32)");
    SendMessageW(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"8 Byte (int64)");
    SendMessageW(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"Float");
    SendMessageW(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"Double");
    SendMessageW(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"String (ASCII)");
    SendMessageW(g_hTypeCombo, CB_ADDSTRING, 0, (LPARAM)L"String (Unicode)");
    SendMessageW(g_hTypeCombo, CB_SETCURSEL, 0, 0);

    CreateWindowW(L"STATIC", L"Suchwert:",
        WS_CHILD | WS_VISIBLE,
        360, 70, 80, 20, hwnd, nullptr, hInstance, nullptr);

    g_hValueInput = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | WS_HSCROLL | WS_VSCROLL,
        450, 67, 280, 60, hwnd, (HMENU)IDC_VALUE_INPUT, hInstance, nullptr);
    SendMessage(g_hValueInput, EM_SETLIMITTEXT, 32768, 0);

    // Scan Buttons - bessere Anordnung
    CreateWindowW(L"BUTTON", L"🎯 Erster Scan",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 140, 120, 35, hwnd, (HMENU)IDC_BTN_FIRST_SCAN, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"🔍 Nächster Scan",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        490, 140, 120, 35, hwnd, (HMENU)IDC_BTN_NEXT_SCAN, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"📈 Geändert",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        620, 140, 120, 35, hwnd, (HMENU)IDC_BTN_CHANGED, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"📉 Ungeändert",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 180, 120, 35, hwnd, (HMENU)IDC_BTN_UNCHANGED, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"🗑️ Zurücksetzen",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        490, 180, 120, 35, hwnd, (HMENU)IDC_BTN_RESET, hInstance, nullptr);

    // Memory Editor Group (Rechts)
    CreateWindowW(L"BUTTON", L"⚙️ Speicher Editor",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        770, 15, 380, 220, hwnd, nullptr, hInstance, nullptr);

    CreateWindowW(L"STATIC", L"Adresse (Hex):",
        WS_CHILD | WS_VISIBLE,
        780, 40, 100, 20, hwnd, nullptr, hInstance, nullptr);

    g_hAddressInput = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_UPPERCASE,
        890, 37, 250, 28, hwnd, (HMENU)IDC_ADDRESS_INPUT, hInstance, nullptr);

    CreateWindowW(L"STATIC", L"Neuer Wert:",
        WS_CHILD | WS_VISIBLE,
        780, 75, 100, 20, hwnd, nullptr, hInstance, nullptr);

    g_hNewValueInput = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | WS_HSCROLL | WS_VSCROLL,
        890, 72, 250, 60, hwnd, (HMENU)IDC_NEW_VALUE_INPUT, hInstance, nullptr);
    SendMessage(g_hNewValueInput, EM_SETLIMITTEXT, 32768, 0);

    CreateWindowW(L"BUTTON", L"✏️ Wert schreiben",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        780, 145, 170, 35, hwnd, (HMENU)IDC_BTN_WRITE, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"👁️ Wert lesen",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        960, 145, 170, 35, hwnd, (HMENU)IDC_BTN_READ, hInstance, nullptr);

    // =================================================================================
    // MAIN CONTENT: Results List
    // =================================================================================

    CreateWindowW(L"STATIC", L"📋 Gefundene Adressen & Werte:",
        WS_CHILD | WS_VISIBLE,
        15, 250, 200, 25, hwnd, nullptr, hInstance, nullptr);

    g_hResultList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        15, 275, 1350, 420, hwnd, (HMENU)IDC_RESULT_LIST, hInstance, nullptr);

    // Enhanced list view columns
    LVCOLUMNW lvc = {};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    lvc.fmt = LVCFMT_LEFT;

    lvc.cx = 250;
    lvc.pszText = (LPWSTR)L"📍 Speicheradresse";
    ListView_InsertColumn(g_hResultList, 0, &lvc);

    lvc.cx = 400;
    lvc.pszText = (LPWSTR)L"💾 Aktueller Wert";
    ListView_InsertColumn(g_hResultList, 1, &lvc);

    lvc.cx = 150;
    lvc.pszText = (LPWSTR)L"📊 Datentyp";
    ListView_InsertColumn(g_hResultList, 2, &lvc);

    // Enable full row selection and grid lines
    ListView_SetExtendedListViewStyle(g_hResultList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);

    // =================================================================================
    // BOTTOM: Status & Process Info
    // =================================================================================

    g_hProcessLabel = CreateWindowW(L"STATIC", L"🎯 Ausgewählter Prozess: Keiner",
        WS_CHILD | WS_VISIBLE,
        15, 710, 600, 30, hwnd, nullptr, hInstance, nullptr);

    g_hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hwnd, (HMENU)IDC_STATUS_BAR, hInstance, nullptr);
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

    // Dynamische Allokierung für große Eingaben
    int valueLength = GetWindowTextLengthW(g_hValueInput);
    std::vector<wchar_t> buffer(valueLength + 1);
    GetWindowTextW(g_hValueInput, buffer.data(), valueLength + 1);

    // Get selected scan type
    int typeIndex = (int)SendMessage(g_hTypeCombo, CB_GETCURSEL, 0, 0);
    g_currentScanType = static_cast<ScanValueType>(typeIndex);

    // Check if input is empty
    bool isEmptyInput = (valueLength == 0);

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
                int32_t value = _wtoi(buffer.data());
                g_currentMatches = g_pScanner->scanForValue(value);
            }
            break;

        case ScanValueType::INT64:
            if (isEmptyInput) {
                auto matches = g_pScanner->scanAllValues<int64_t>();
                for (const auto& m : matches) {
                    MemoryMatch<int32_t> converted;
                    converted.address = m.address;
                    converted.value = static_cast<int32_t>(m.value);
                    g_currentMatches.push_back(converted);
                }
            } else {
                int64_t value = _wtoi64(buffer.data());
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
                float value = std::stof(buffer.data());
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
                double value = std::stod(buffer.data());
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
                std::vector<char> asciiBuffer(valueLength + 1);
                wcstombs(asciiBuffer.data(), buffer.data(), valueLength + 1);
                std::string searchStr(asciiBuffer.data());
                g_stringMatches = g_pScanner->scanForString(searchStr);
            }
            break;

        case ScanValueType::STRING_UNICODE:
            if (!isEmptyInput) {
                std::wstring searchStr(buffer.data());
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

    // Dynamische Allokierung für große Eingaben
    int valueLength = GetWindowTextLengthW(g_hValueInput);
    std::vector<wchar_t> buffer(valueLength + 1);
    GetWindowTextW(g_hValueInput, buffer.data(), valueLength + 1);

    UpdateStatusBar(L"Filtere Ergebnisse...");
    UpdateWindow(g_hMainWindow);

    // Filter based on current scan type
    switch (g_currentScanType) {
        case ScanValueType::INT32: {
            int32_t value = _wtoi(buffer.data());
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
            int64_t value = _wtoi64(buffer.data());
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
            float value = std::stof(buffer.data());
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
            double value = std::stod(buffer.data());
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
            std::vector<char> asciiBuffer(valueLength + 1);
            wcstombs(asciiBuffer.data(), buffer.data(), valueLength + 1);
            std::string searchStr(asciiBuffer.data());

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
            std::wstring searchStr(buffer.data());

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
    size_t maxDisplay = std::min<size_t>(totalMatches, 2000); // Erhöht auf 2000 für bessere Performance

    // Reserve space for better performance
    g_displayedAddresses.reserve(maxDisplay);
    g_displayedValues.reserve(maxDisplay);

    // Add int32 matches
    for (size_t i = 0; i < g_currentMatches.size() && g_displayedAddresses.size() < maxDisplay; i++) {
        std::wstringstream ss;
        ss << L"0x" << std::hex << std::uppercase << std::setw(16) << std::setfill(L'0') << g_currentMatches[i].address;

        // Add to ListView
        LVITEMW lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)g_displayedAddresses.size();
        lvi.pszText = const_cast<LPWSTR>(ss.str().c_str());
        ListView_InsertItem(g_hResultList, &lvi);

        // Wert
        std::wstring valueStr = std::to_wstring(g_currentMatches[i].value);
        ListView_SetItemText(g_hResultList, (int)g_displayedAddresses.size(), 1, const_cast<LPWSTR>(valueStr.c_str()));

        // Datentyp
        std::wstring typeStr;
        switch (g_currentScanType) {
            case ScanValueType::INT32: typeStr = L"INT32"; break;
            case ScanValueType::INT64: typeStr = L"INT64"; break;
            case ScanValueType::FLOAT: typeStr = L"FLOAT"; break;
            case ScanValueType::DOUBLE: typeStr = L"DOUBLE"; break;
            default: typeStr = L"NUMERIC"; break;
        }
        ListView_SetItemText(g_hResultList, (int)g_displayedAddresses.size(), 2, const_cast<LPWSTR>(typeStr.c_str()));

        g_displayedAddresses.push_back(ss.str());
        g_displayedValues.push_back(valueStr);
    }

    // Add ASCII string matches
    for (size_t i = 0; i < g_stringMatches.size() && g_displayedAddresses.size() < maxDisplay; i++) {
        std::wstringstream ss;
        ss << L"0x" << std::hex << std::uppercase << std::setw(16) << std::setfill(L'0') << g_stringMatches[i].address;

        // Add to ListView
        LVITEMW lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)g_displayedAddresses.size();
        lvi.pszText = const_cast<LPWSTR>(ss.str().c_str());
        ListView_InsertItem(g_hResultList, &lvi);

        // Convert ASCII string to wstring for display
        std::wstring wstr(g_stringMatches[i].value.begin(), g_stringMatches[i].value.end());
        // Truncate long strings for display
        if (wstr.length() > 100) {
            wstr = wstr.substr(0, 97) + L"...";
        }
        ListView_SetItemText(g_hResultList, (int)g_displayedAddresses.size(), 1, const_cast<LPWSTR>(wstr.c_str()));

        // Datentyp
        ListView_SetItemText(g_hResultList, (int)g_displayedAddresses.size(), 2, L"ASCII");

        g_displayedAddresses.push_back(ss.str());
        g_displayedValues.push_back(wstr);
    }

    // Add Unicode string matches
    for (size_t i = 0; i < g_wstringMatches.size() && g_displayedAddresses.size() < maxDisplay; i++) {
        std::wstringstream ss;
        ss << L"0x" << std::hex << std::uppercase << std::setw(16) << std::setfill(L'0') << g_wstringMatches[i].address;

        // Add to ListView
        LVITEMW lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)g_displayedAddresses.size();
        lvi.pszText = const_cast<LPWSTR>(ss.str().c_str());
        ListView_InsertItem(g_hResultList, &lvi);

        // Truncate long strings for display
        std::wstring displayValue = g_wstringMatches[i].value;
        if (displayValue.length() > 100) {
            displayValue = displayValue.substr(0, 97) + L"...";
        }
        ListView_SetItemText(g_hResultList, (int)g_displayedAddresses.size(), 1, const_cast<LPWSTR>(displayValue.c_str()));

        // Datentyp
        ListView_SetItemText(g_hResultList, (int)g_displayedAddresses.size(), 2, L"UNICODE");

        g_displayedAddresses.push_back(ss.str());
        g_displayedValues.push_back(displayValue);
    }

    if (totalMatches > maxDisplay) {
        UpdateStatusBar(L"📊 Zeige " + std::to_wstring(maxDisplay) + L" von " +
                       std::to_wstring(totalMatches) + L" Treffern");
    } else if (totalMatches > 0) {
        UpdateStatusBar(L"📊 " + std::to_wstring(totalMatches) + L" Treffer gefunden");
    } else {
        UpdateStatusBar(L"❌ Keine Treffer gefunden");
    }
}

void WriteValue() {
    if (!g_pScanner) {
        MessageBoxW(g_hMainWindow, L"Bitte hängen Sie sich zuerst an einen Prozess an!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t addrBuffer[32];
    GetWindowTextW(g_hAddressInput, addrBuffer, 32);

    // Dynamische Allokierung für große Eingaben
    int valueLength = GetWindowTextLengthW(g_hNewValueInput);
    if (valueLength == 0) {
        MessageBoxW(g_hMainWindow, L"Bitte geben Sie Adresse und Wert ein!", L"Fehler", MB_OK | MB_ICONWARNING);
        return;
    }

    std::vector<wchar_t> valueBuffer(valueLength + 1);
    GetWindowTextW(g_hNewValueInput, valueBuffer.data(), valueLength + 1);

    if (wcslen(addrBuffer) == 0) {
        MessageBoxW(g_hMainWindow, L"Bitte geben Sie Adresse und Wert ein!", L"Fehler", MB_OK | MB_ICONWARNING);
        return;
    }

    uintptr_t address = std::stoull(addrBuffer, nullptr, 16);
    bool success = false;

    // Write based on current scan type
    switch (g_currentScanType) {
        case ScanValueType::INT32: {
            int32_t value = _wtoi(valueBuffer.data());
            success = g_pScanner->writeValue(address, value);
            break;
        }
        case ScanValueType::INT64: {
            int64_t value = _wtoi64(valueBuffer.data());
            success = g_pScanner->writeValue(address, value);
            break;
        }
        case ScanValueType::FLOAT: {
            float value = std::stof(valueBuffer.data());
            success = g_pScanner->writeValue(address, value);
            break;
        }
        case ScanValueType::DOUBLE: {
            double value = std::stod(valueBuffer.data());
            success = g_pScanner->writeValue(address, value);
            break;
        }
        case ScanValueType::STRING_ASCII: {
            // Convert wchar_t to ASCII string
            std::vector<char> asciiBuffer(valueLength + 1);
            wcstombs(asciiBuffer.data(), valueBuffer.data(), valueLength + 1);
            std::string str(asciiBuffer.data());
            success = g_pScanner->writeMemory(address, str.c_str(), str.length());
            break;
        }
        case ScanValueType::STRING_UNICODE: {
            std::wstring str(valueBuffer.data());
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
            // Read up to 8KB as ASCII string
            std::vector<char> buffer(8192, 0);
            if (g_pScanner->readMemory(address, buffer.data(), buffer.size() - 1)) {
                std::string str(buffer.data());
                std::wstring wstr(str.begin(), str.end());
                ss << wstr;
                SetWindowTextW(g_hNewValueInput, wstr.c_str());
                success = true;
            }
            break;
        }
        case ScanValueType::STRING_UNICODE: {
            // Read up to 4KB wchars as Unicode string
            std::vector<wchar_t> buffer(4096, 0);
            if (g_pScanner->readMemory(address, buffer.data(), (buffer.size() - 1) * sizeof(wchar_t))) {
                std::wstring str(buffer.data());
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

void UpdateInputLimitForAddress(uintptr_t address) {
    // Keine Limitierung mehr - die Felder haben bereits ein großzügiges Limit von 32KB
    // Diese Funktion wird beibehalten für zukünftige Erweiterungen, tut aber nichts mehr

    if (!g_pScanner) {
        return;
    }

    // Get the size of the memory region at this address for info purposes
    size_t regionSize = g_pScanner->getRegionSizeAtAddress(address);

    if (regionSize > 0) {
        // Update status bar with info (optional)
        std::wstringstream ss;
        ss << L"Verfügbarer Speicher an Adresse: " << regionSize << L" Bytes";
        UpdateStatusBar(ss.str());
    }
}

void OnAddressInputChanged() {
    wchar_t addrBuffer[32];
    GetWindowTextW(g_hAddressInput, addrBuffer, 32);

    if (wcslen(addrBuffer) > 0) {
        try {
            uintptr_t address = std::stoull(addrBuffer, nullptr, 16);
            UpdateInputLimitForAddress(address);
        } catch (...) {
            // Invalid address format - ignore
        }
    }
}

void OnResultListItemSelected() {
    int selectedIndex = ListView_GetNextItem(g_hResultList, -1, LVNI_SELECTED);

    if (selectedIndex == -1) {
        return; // No item selected
    }

    // Get address text from the selected item
    wchar_t addressText[32] = {0};
    ListView_GetItemText(g_hResultList, selectedIndex, 0, addressText, 32);

    // Set the address in the address input field
    SetWindowTextW(g_hAddressInput, addressText);

    // Parse the address and update input limit
    if (wcslen(addressText) > 0) {
        try {
            uintptr_t address = std::stoull(addressText, nullptr, 16);
            UpdateInputLimitForAddress(address);

            // Also get the current value and display it
            wchar_t valueText[256] = {0};
            ListView_GetItemText(g_hResultList, selectedIndex, 1, valueText, 256);
            SetWindowTextW(g_hNewValueInput, valueText);
        } catch (...) {
            // Invalid address
        }
    }
}
#else
#include <iostream>
int main() {
    std::cout << "Dieses Programm funktioniert nur unter Windows!\n";
    return 1;
}
#endif
