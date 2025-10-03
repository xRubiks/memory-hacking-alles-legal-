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

#endif

