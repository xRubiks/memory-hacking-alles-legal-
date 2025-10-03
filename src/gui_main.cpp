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
std::vector<std::wstring> g_displayedAddresses;
std::vector<std::wstring> g_displayedValues;
std::wstring g_currentProcessName = L"";
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

    CreateWindowW(L"STATIC", L"Wert:",
        WS_CHILD | WS_VISIBLE,
        330, 35, 50, 20, hwnd, nullptr, hInstance, nullptr);

    g_hValueInput = CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        385, 32, 220, 25, hwnd, (HMENU)IDC_VALUE_INPUT, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Erster Scan",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        330, 65, 135, 30, hwnd, (HMENU)IDC_BTN_FIRST_SCAN, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Nächster Scan",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        470, 65, 135, 30, hwnd, (HMENU)IDC_BTN_NEXT_SCAN, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Geändert",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        330, 100, 135, 30, hwnd, (HMENU)IDC_BTN_CHANGED, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Ungeändert",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        470, 100, 135, 30, hwnd, (HMENU)IDC_BTN_UNCHANGED, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Scan zurücksetzen",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        330, 135, 275, 30, hwnd, (HMENU)IDC_BTN_RESET, hInstance, nullptr);

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
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
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

    wchar_t buffer[32];
    GetWindowTextW(g_hValueInput, buffer, 32);

    // Check if input is empty
    bool isEmptyInput = (wcslen(buffer) == 0);

    UpdateStatusBar(L"Scanne Speicher... Bitte warten...");
    UpdateWindow(g_hMainWindow);

    if (isEmptyInput) {
        // Scan ALL values (Unknown Initial Value)
        g_currentMatches = g_pScanner->scanAllValues<int32_t>();
    } else {
        // Scan for specific value
        int32_t value = _wtoi(buffer);
        g_currentMatches = g_pScanner->scanForValue(value);
    }

    g_hasInitialScan = true;

    UpdateResultList();

    // Show status
    std::wstringstream status;
    status << L"✓ Scan abgeschlossen! Gefunden: " << g_currentMatches.size() << L" Adressen";
    UpdateStatusBar(status.str());
}

void PerformNextScan() {
    if (!g_hasInitialScan || g_currentMatches.empty()) {
        MessageBoxW(g_hMainWindow, L"Führen Sie zuerst einen ersten Scan durch!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t buffer[32];
    GetWindowTextW(g_hValueInput, buffer, 32);
    int32_t value = _wtoi(buffer);

    UpdateStatusBar(L"Filtere Ergebnisse...");
    UpdateWindow(g_hMainWindow);

    g_currentMatches = g_pScanner->filterByValue(g_currentMatches, value);

    UpdateResultList();
    UpdateStatusBar(L"✓ Scan abgeschlossen! Verbleibend: " + std::to_wstring(g_currentMatches.size()) + L" Adressen");
}

void PerformChangedScan() {
    if (!g_hasInitialScan || g_currentMatches.empty()) {
        MessageBoxW(g_hMainWindow, L"Führen Sie zuerst einen ersten Scan durch!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    UpdateStatusBar(L"Scanne nach geänderten Werten...");
    UpdateWindow(g_hMainWindow);

    g_currentMatches = g_pScanner->filterByChanged(g_currentMatches);

    UpdateResultList();
    UpdateStatusBar(L"✓ Scan abgeschlossen! Verbleibend: " + std::to_wstring(g_currentMatches.size()) + L" Adressen");
}

void PerformUnchangedScan() {
    if (!g_hasInitialScan || g_currentMatches.empty()) {
        MessageBoxW(g_hMainWindow, L"Führen Sie zuerst einen ersten Scan durch!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    UpdateStatusBar(L"Scanne nach ungeänderten Werten...");
    UpdateWindow(g_hMainWindow);

    g_currentMatches = g_pScanner->filterByUnchanged(g_currentMatches);

    UpdateResultList();
    UpdateStatusBar(L"✓ Scan abgeschlossen! Verbleibend: " + std::to_wstring(g_currentMatches.size()) + L" Adressen");
}

void UpdateResultList() {
    ListView_DeleteAllItems(g_hResultList);

    // Clear old display strings
    g_displayedAddresses.clear();
    g_displayedValues.clear();

    size_t maxDisplay = std::min<size_t>(g_currentMatches.size(), 1000);

    // Reserve space for better performance
    g_displayedAddresses.reserve(maxDisplay);
    g_displayedValues.reserve(maxDisplay);

    for (size_t i = 0; i < maxDisplay; i++) {
        // Create address string
        std::wstringstream ss;
        ss << L"0x" << std::hex << std::setw(16) << std::setfill(L'0') << g_currentMatches[i].address;
        g_displayedAddresses.push_back(ss.str());

        // Use the value from the scan directly
        int32_t displayValue = g_currentMatches[i].value;
        g_displayedValues.push_back(std::to_wstring(displayValue));

        // Insert into ListView
        LVITEMW lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = (int)i;
        lvi.pszText = const_cast<LPWSTR>(g_displayedAddresses[i].c_str());
        ListView_InsertItem(g_hResultList, &lvi);

        // Set value column
        ListView_SetItemText(g_hResultList, (int)i, 1, const_cast<LPWSTR>(g_displayedValues[i].c_str()));
    }

    if (g_currentMatches.size() > maxDisplay) {
        UpdateStatusBar(L"Zeige " + std::to_wstring(maxDisplay) + L" von " +
                       std::to_wstring(g_currentMatches.size()) + L" Treffern");
    }
}

void WriteValue() {
    if (!g_pScanner) {
        MessageBoxW(g_hMainWindow, L"Bitte hängen Sie sich zuerst an einen Prozess an!", L"Fehler", MB_OK | MB_ICONERROR);
        return;
    }

    wchar_t addrBuffer[32];
    wchar_t valueBuffer[32];

    GetWindowTextW(g_hAddressInput, addrBuffer, 32);
    GetWindowTextW(g_hNewValueInput, valueBuffer, 32);

    if (wcslen(addrBuffer) == 0 || wcslen(valueBuffer) == 0) {
        MessageBoxW(g_hMainWindow, L"Bitte geben Sie Adresse und Wert ein!", L"Fehler", MB_OK | MB_ICONWARNING);
        return;
    }

    uintptr_t address = std::stoull(addrBuffer, nullptr, 16);
    int32_t value = _wtoi(valueBuffer);

    if (g_pScanner->writeValue(address, value)) {
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
    int32_t value;

    if (g_pScanner->readValue(address, value)) {
        std::wstringstream ss;
        ss << L"Wert an Adresse 0x" << std::hex << address << L":\n\n" << std::dec << value;
        MessageBoxW(g_hMainWindow, ss.str().c_str(), L"Wert gelesen", MB_OK | MB_ICONINFORMATION);

        SetWindowTextW(g_hNewValueInput, std::to_wstring(value).c_str());
        UpdateStatusBar(L"✓ Wert erfolgreich gelesen: " + std::to_wstring(value));
    } else {
        MessageBoxW(g_hMainWindow, L"Fehler beim Lesen des Werts!", L"Fehler", MB_OK | MB_ICONERROR);
        UpdateStatusBar(L"✗ Fehler beim Lesen des Werts");
    }
}

void ResetScan() {
    g_currentMatches.clear();
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
