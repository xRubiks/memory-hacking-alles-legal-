#ifdef _WIN32
#include "memory_scanner.h"
#include <iostream>

MemoryScanner::MemoryScanner(HANDLE processHandle)
    : m_processHandle(processHandle) {
}

MemoryScanner::~MemoryScanner() {
    // Don't close the handle here - it's managed externally
}

std::vector<MemoryRegion> MemoryScanner::getReadableRegions() {
    std::vector<MemoryRegion> regions;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    uintptr_t address = (uintptr_t)sysInfo.lpMinimumApplicationAddress;
    uintptr_t maxAddress = (uintptr_t)sysInfo.lpMaximumApplicationAddress;

    MEMORY_BASIC_INFORMATION mbi;

    while (address < maxAddress) {
        if (VirtualQueryEx(m_processHandle, (LPCVOID)address, &mbi, sizeof(mbi)) == 0) {
            break;
        }

        MemoryRegion region;
        region.baseAddress = (uintptr_t)mbi.BaseAddress;
        region.size = mbi.RegionSize;
        region.protection = mbi.Protect;
        region.state = mbi.State;
        region.type = mbi.Type;

        if (isReadableRegion(region)) {
            regions.push_back(region);
        }

        address += mbi.RegionSize;
    }

    return regions;
}

bool MemoryScanner::isReadableRegion(const MemoryRegion& region) {
    // Check if committed
    if (region.state != MEM_COMMIT) return false;

    // Check if readable (and not guard page)
    if (region.protection & PAGE_NOACCESS) return false;
    if (region.protection & PAGE_GUARD) return false;

    // Must have at least read access
    return (region.protection & PAGE_READONLY) ||
           (region.protection & PAGE_READWRITE) ||
           (region.protection & PAGE_WRITECOPY) ||
           (region.protection & PAGE_EXECUTE_READ) ||
           (region.protection & PAGE_EXECUTE_READWRITE) ||
           (region.protection & PAGE_EXECUTE_WRITECOPY);
}

bool MemoryScanner::readMemory(uintptr_t address, void* buffer, size_t size) {
    SIZE_T bytesRead;
    return ReadProcessMemory(m_processHandle, (LPCVOID)address, buffer, size, &bytesRead)
           && bytesRead == size;
}

bool MemoryScanner::writeMemory(uintptr_t address, const void* buffer, size_t size) {
    SIZE_T bytesWritten;
    return WriteProcessMemory(m_processHandle, (LPVOID)address, buffer, size, &bytesWritten)
           && bytesWritten == size;
}

std::vector<MemoryMatch<std::string>> MemoryScanner::scanForString(const std::string& value) {
    std::vector<MemoryMatch<std::string>> matches;
    auto regions = getReadableRegions();

    for (const auto& region : regions) {
        // Skip if region is too small
        if (region.size < value.length()) continue;

        // Allocate buffer for this region
        std::vector<uint8_t> buffer(region.size);

        if (readMemory(region.baseAddress, buffer.data(), region.size)) {
            // Search for the string in the buffer
            for (size_t i = 0; i <= region.size - value.length(); i++) {
                bool found = true;
                for (size_t j = 0; j < value.length(); j++) {
                    if (buffer[i + j] != static_cast<uint8_t>(value[j])) {
                        found = false;
                        break;
                    }
                }

                if (found) {
                    MemoryMatch<std::string> match;
                    match.address = region.baseAddress + i;
                    match.value = value;
                    matches.push_back(match);
                }
            }
        }
    }

    return matches;
}

std::vector<MemoryMatch<std::wstring>> MemoryScanner::scanForWideString(const std::wstring& value) {
    std::vector<MemoryMatch<std::wstring>> matches;
    auto regions = getReadableRegions();

    size_t byteLength = value.length() * sizeof(wchar_t);

    for (const auto& region : regions) {
        // Skip if region is too small
        if (region.size < byteLength) continue;

        // Allocate buffer for this region
        std::vector<uint8_t> buffer(region.size);

        if (readMemory(region.baseAddress, buffer.data(), region.size)) {
            // Search for the wide string in the buffer
            for (size_t i = 0; i <= region.size - byteLength; i++) {
                bool found = true;
                for (size_t j = 0; j < value.length(); j++) {
                    wchar_t currentChar;
                    std::memcpy(&currentChar, &buffer[i + j * sizeof(wchar_t)], sizeof(wchar_t));
                    if (currentChar != value[j]) {
                        found = false;
                        break;
                    }
                }

                if (found) {
                    MemoryMatch<std::wstring> match;
                    match.address = region.baseAddress + i;
                    match.value = value;
                    matches.push_back(match);
                }
            }
        }
    }

    return matches;
}

#endif
