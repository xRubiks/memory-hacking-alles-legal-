#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <limits>
#include <vector>

#ifdef _WIN32
#include "src/process_utils.h"
#include "src/memory_scanner.h"

// Template to handle different data types
template<typename T>
class ScanSession {
public:
    std::vector<MemoryMatch<T>> currentMatches;
    bool hasInitialScan = false;

    void displayMatches(size_t maxDisplay = 20) {
        if (currentMatches.empty()) {
            std::cout << "Keine Treffer gefunden.\n";
            return;
        }

        std::cout << "\nGefundene Adressen: " << currentMatches.size() << "\n";
        std::cout << "Zeige ersten " << std::min(currentMatches.size(), maxDisplay) << " Treffer:\n";
        std::cout << std::string(60, '-') << "\n";
        std::cout << std::setw(18) << "Adresse" << " | " << "Wert\n";
        std::cout << std::string(60, '-') << "\n";

        for (size_t i = 0; i < std::min(currentMatches.size(), maxDisplay); i++) {
            std::cout << "0x" << std::hex << std::setw(16) << std::setfill('0')
                      << currentMatches[i].address << " | " << std::dec
                      << currentMatches[i].value << "\n";
        }
        std::cout << std::string(60, '-') << "\n";
    }
};

void displayMenu() {
    std::cout << "\n╔════════════════════════════════════════════╗\n";
    std::cout << "║     Memory Scanner - CheatEngine Style    ║\n";
    std::cout << "╚════════════════════════════════════════════╝\n";
    std::cout << "1. Prozess auswählen\n";
    std::cout << "2. Ersten Scan starten (Initial Scan)\n";
    std::cout << "3. Nächster Scan (exakter Wert)\n";
    std::cout << "4. Nächster Scan (geänderter Wert)\n";
    std::cout << "5. Nächster Scan (ungeänderter Wert)\n";
    std::cout << "6. Gefundene Adressen anzeigen\n";
    std::cout << "7. Wert an Adresse ändern\n";
    std::cout << "8. Wert an Adresse lesen\n";
    std::cout << "9. Scan zurücksetzen\n";
    std::cout << "0. Beenden\n";
    std::cout << "─────────────────────────────────────────────\n";
    std::cout << "Wählen Sie eine Option: ";
}

HANDLE selectProcess() {
    std::cout << "\n=== Prozess auswählen ===\n";
    std::cout << "Geben Sie den Namen oder Teil des Namens ein: ";

    std::wstring searchTerm;
    std::string input;
    std::getline(std::cin, input);

    // Convert to wide string
    searchTerm = std::wstring(input.begin(), input.end());

    auto processes = findProcessesBySubstring(searchTerm);

    if (processes.empty()) {
        std::cout << "Keine Prozesse gefunden.\n";
        return nullptr;
    }

    std::cout << "\nGefundene Prozesse:\n";
    std::cout << std::string(60, '-') << "\n";
    std::cout << std::setw(6) << "Nr" << " | "
              << std::setw(8) << "PID" << " | "
              << "Name\n";
    std::cout << std::string(60, '-') << "\n";

    for (size_t i = 0; i < processes.size(); i++) {
        std::cout << std::setw(6) << i + 1 << " | "
                  << std::setw(8) << processes[i].pid << " | "
                  << wideToUtf8(processes[i].exeName) << "\n";
    }
    std::cout << std::string(60, '-') << "\n";

    std::cout << "Wählen Sie einen Prozess (Nummer): ";
    size_t choice;
    std::cin >> choice;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if (choice < 1 || choice > processes.size()) {
        std::cout << "Ungültige Auswahl.\n";
        return nullptr;
    }

    HANDLE hProcess = openProcessBasic(processes[choice - 1].pid, true);
    if (hProcess == nullptr) {
        std::cout << "Fehler beim Öffnen des Prozesses. Stellen Sie sicher, dass Sie Administrator-Rechte haben.\n";
        return nullptr;
    }

    std::cout << "✓ Prozess erfolgreich geöffnet: "
              << wideToUtf8(processes[choice - 1].exeName)
              << " (PID: " << processes[choice - 1].pid << ")\n";

    return hProcess;
}

template<typename T>
void performInitialScan(MemoryScanner& scanner, ScanSession<T>& session) {
    std::cout << "\n=== Erster Scan ===\n";
    std::cout << "Geben Sie den Wert ein, den Sie suchen: ";

    T value;
    std::cin >> value;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::cout << "Scanne Speicher...\n";
    session.currentMatches = scanner.scanForValue(value);
    session.hasInitialScan = true;

    std::cout << "✓ Scan abgeschlossen! Gefunden: " << session.currentMatches.size() << " Adressen\n";
    session.displayMatches();
}

template<typename T>
void performNextScan(MemoryScanner& scanner, ScanSession<T>& session) {
    if (!session.hasInitialScan || session.currentMatches.empty()) {
        std::cout << "Führen Sie zuerst einen ersten Scan durch!\n";
        return;
    }

    std::cout << "\n=== Nächster Scan (exakter Wert) ===\n";
    std::cout << "Geben Sie den neuen Wert ein: ";

    T value;
    std::cin >> value;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::cout << "Filtere Ergebnisse...\n";
    session.currentMatches = scanner.filterByValue(session.currentMatches, value);

    std::cout << "✓ Scan abgeschlossen! Verbleibend: " << session.currentMatches.size() << " Adressen\n";
    session.displayMatches();
}

template<typename T>
void performChangedScan(MemoryScanner& scanner, ScanSession<T>& session) {
    if (!session.hasInitialScan || session.currentMatches.empty()) {
        std::cout << "Führen Sie zuerst einen ersten Scan durch!\n";
        return;
    }

    std::cout << "\n=== Scan nach geänderten Werten ===\n";
    std::cout << "Filtere Adressen mit geänderten Werten...\n";

    session.currentMatches = scanner.filterByChanged(session.currentMatches);

    std::cout << "✓ Scan abgeschlossen! Verbleibend: " << session.currentMatches.size() << " Adressen\n";
    session.displayMatches();
}

template<typename T>
void performUnchangedScan(MemoryScanner& scanner, ScanSession<T>& session) {
    if (!session.hasInitialScan || session.currentMatches.empty()) {
        std::cout << "Führen Sie zuerst einen ersten Scan durch!\n";
        return;
    }

    std::cout << "\n=== Scan nach ungeänderten Werten ===\n";
    std::cout << "Filtere Adressen mit ungeänderten Werten...\n";

    session.currentMatches = scanner.filterByUnchanged(session.currentMatches);

    std::cout << "✓ Scan abgeschlossen! Verbleibend: " << session.currentMatches.size() << " Adressen\n";
    session.displayMatches();
}

template<typename T>
void writeValueToAddress(MemoryScanner& scanner) {
    std::cout << "\n=== Wert schreiben ===\n";
    std::cout << "Geben Sie die Adresse ein (hex, z.B. 0x12345678): ";

    std::string addrStr;
    std::cin >> addrStr;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    uintptr_t address;
    std::stringstream ss;
    ss << std::hex << addrStr;
    ss >> address;

    std::cout << "Geben Sie den neuen Wert ein: ";
    T value;
    std::cin >> value;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if (scanner.writeValue(address, value)) {
        std::cout << "✓ Wert erfolgreich geschrieben!\n";
    } else {
        std::cout << "✗ Fehler beim Schreiben des Werts.\n";
    }
}

template<typename T>
void readValueFromAddress(MemoryScanner& scanner) {
    std::cout << "\n=== Wert lesen ===\n";
    std::cout << "Geben Sie die Adresse ein (hex, z.B. 0x12345678): ";

    std::string addrStr;
    std::cin >> addrStr;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    uintptr_t address;
    std::stringstream ss;
    ss << std::hex << addrStr;
    ss >> address;

    T value;
    if (scanner.readValue(address, value)) {
        std::cout << "✓ Wert an Adresse 0x" << std::hex << address
                  << ": " << std::dec << value << "\n";
    } else {
        std::cout << "✗ Fehler beim Lesen des Werts.\n";
    }
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════╗\n";
    std::cout << "║  Memory Scanner - CheatEngine für C++        ║\n";
    std::cout << "║  Entwickelt für Windows (Win32 API)          ║\n";
    std::cout << "╚═══════════════════════════════════════════════╝\n\n";

    HANDLE hProcess = nullptr;
    MemoryScanner* scanner = nullptr;

    // For now, we'll work with 4-byte integers (most common for games)
    ScanSession<int32_t> session;

    while (true) {
        displayMenu();

        int choice;
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (choice) {
            case 1: {
                if (hProcess != nullptr) {
                    CloseHandle(hProcess);
                    delete scanner;
                    scanner = nullptr;
                }
                hProcess = selectProcess();
                if (hProcess != nullptr) {
                    scanner = new MemoryScanner(hProcess);
                    session = ScanSession<int32_t>(); // Reset session
                }
                break;
            }

            case 2: {
                if (scanner == nullptr) {
                    std::cout << "Bitte wählen Sie zuerst einen Prozess aus!\n";
                    break;
                }
                performInitialScan(*scanner, session);
                break;
            }

            case 3: {
                if (scanner == nullptr) {
                    std::cout << "Bitte wählen Sie zuerst einen Prozess aus!\n";
                    break;
                }
                performNextScan(*scanner, session);
                break;
            }

            case 4: {
                if (scanner == nullptr) {
                    std::cout << "Bitte wählen Sie zuerst einen Prozess aus!\n";
                    break;
                }
                performChangedScan(*scanner, session);
                break;
            }

            case 5: {
                if (scanner == nullptr) {
                    std::cout << "Bitte wählen Sie zuerst einen Prozess aus!\n";
                    break;
                }
                performUnchangedScan(*scanner, session);
                break;
            }

            case 6: {
                if (scanner == nullptr) {
                    std::cout << "Bitte wählen Sie zuerst einen Prozess aus!\n";
                    break;
                }
                std::cout << "\nWie viele Treffer sollen angezeigt werden? ";
                size_t count;
                std::cin >> count;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                session.displayMatches(count);
                break;
            }

            case 7: {
                if (scanner == nullptr) {
                    std::cout << "Bitte wählen Sie zuerst einen Prozess aus!\n";
                    break;
                }
                writeValueToAddress<int32_t>(*scanner);
                break;
            }

            case 8: {
                if (scanner == nullptr) {
                    std::cout << "Bitte wählen Sie zuerst einen Prozess aus!\n";
                    break;
                }
                readValueFromAddress<int32_t>(*scanner);
                break;
            }

            case 9: {
                session = ScanSession<int32_t>();
                std::cout << "✓ Scan wurde zurückgesetzt.\n";
                break;
            }

            case 0: {
                std::cout << "\nBeende Programm...\n";
                if (scanner != nullptr) {
                    delete scanner;
                }
                if (hProcess != nullptr) {
                    CloseHandle(hProcess);
                }
                return 0;
            }

            default:
                std::cout << "Ungültige Option!\n";
                break;
        }
    }

    return 0;
}

#else
int main() {
    std::cout << "Dieses Programm funktioniert nur unter Windows!\n";
    return 1;
}
#endif

