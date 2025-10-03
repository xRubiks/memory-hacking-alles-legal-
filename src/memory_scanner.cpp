#ifdef _WIN32
#include "memory_scanner.h"
#include <algorithm>

MemoryScanner::MemoryScanner(HANDLE processHandle) : m_processHandle(processHandle) {}

MemoryScanner::~MemoryScanner() {}

std::vector<MemoryRegion> MemoryScanner::getReadableRegions() {
    std::vector<MemoryRegion> regions;

    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t address = 0;

    while (VirtualQueryEx(m_processHandle, (LPCVOID)address, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && isReadableRegion(MemoryRegion{
            (uintptr_t)mbi.BaseAddress, mbi.RegionSize, mbi.Protect, mbi.State, mbi.Type
        })) {
            MemoryRegion region;
            region.baseAddress = (uintptr_t)mbi.BaseAddress;
            region.size = mbi.RegionSize;
            region.protection = mbi.Protect;
            region.state = mbi.State;
            region.type = mbi.Type;
            regions.push_back(region);
        }

        address = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;

        if (address == 0) break;
    }

    return regions;
}

size_t MemoryScanner::getRegionSizeAtAddress(uintptr_t address) {
    MEMORY_BASIC_INFORMATION mbi;

    if (VirtualQueryEx(m_processHandle, (LPCVOID)address, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT) {
            // Calculate the remaining size from the given address to the end of the region
            uintptr_t regionStart = (uintptr_t)mbi.BaseAddress;
            uintptr_t regionEnd = regionStart + mbi.RegionSize;

            if (address >= regionStart && address < regionEnd) {
                return regionEnd - address;
            }
        }
    }

    return 0;
}

bool MemoryScanner::isReadableRegion(const MemoryRegion& region) {
    return (region.protection & PAGE_GUARD) == 0 &&
           (region.protection & PAGE_NOACCESS) == 0 &&
           region.state == MEM_COMMIT;
}

bool MemoryScanner::readMemory(uintptr_t address, void* buffer, size_t size) {
    SIZE_T bytesRead;
    return ReadProcessMemory(m_processHandle, (LPCVOID)address, buffer, size, &bytesRead) && bytesRead == size;
}

bool MemoryScanner::writeMemory(uintptr_t address, const void* buffer, size_t size) {
    SIZE_T bytesWritten;
    return WriteProcessMemory(m_processHandle, (LPVOID)address, buffer, size, &bytesWritten) && bytesWritten == size;
}

std::vector<MemoryMatch<std::string>> MemoryScanner::scanForString(const std::string& value) {
    std::vector<MemoryMatch<std::string>> matches;
    auto regions = getReadableRegions();

    for (const auto& region : regions) {
        if (region.size < value.length()) continue;

        std::vector<uint8_t> buffer(region.size);

        if (readMemory(region.baseAddress, buffer.data(), region.size)) {
            for (size_t i = 0; i <= region.size - value.length(); i++) {
                if (memcmp(&buffer[i], value.c_str(), value.length()) == 0) {
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

    size_t searchSize = value.length() * sizeof(wchar_t);

    for (const auto& region : regions) {
        if (region.size < searchSize) continue;

        std::vector<uint8_t> buffer(region.size);

        if (readMemory(region.baseAddress, buffer.data(), region.size)) {
            for (size_t i = 0; i <= region.size - searchSize; i++) {
                if (memcmp(&buffer[i], value.c_str(), searchSize) == 0) {
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
