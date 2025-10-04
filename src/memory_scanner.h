#pragma once
#ifdef _WIN32
#include <windows.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include <map>

// Represents a memory region
struct MemoryRegion {
    uintptr_t baseAddress;
    size_t size;
    DWORD protection;
    DWORD state;
    DWORD type;
};

// Represents a found memory address with its value
template<typename T>
struct MemoryMatch {
    uintptr_t address;
    T value;
};

struct Module {
    std::string name;
    uintptr_t baseAddress;
    size_t size;
};

// Memory Scanner class
class MemoryScanner {
public:
    explicit MemoryScanner(HANDLE processHandle);
    ~MemoryScanner();

    // Get all readable memory regions
    std::vector<MemoryRegion> getReadableRegions();

    // Get the size of the memory region at a specific address
    size_t getRegionSizeAtAddress(uintptr_t address);


    // Get the size of the memory region at a specific address
    Module* getModuleByAddress(uintptr_t address);

    // Initial scan: find all addresses matching a specific value
    template<typename T>
    std::vector<MemoryMatch<T>> scanForValue(T value);

    // Initial scan: find ALL addresses (unknown initial value)
    template<typename T>
    std::vector<MemoryMatch<T>> scanAllValues();

    // Next scan: filter previous results by new value
    template<typename T>
    std::vector<MemoryMatch<T>> filterByValue(const std::vector<MemoryMatch<T>>& previous, T value);

    // Scan for changed values
    template<typename T>
    std::vector<MemoryMatch<T>> filterByChanged(const std::vector<MemoryMatch<T>>& previous);

    // Scan for unchanged values
    template<typename T>
    std::vector<MemoryMatch<T>> filterByUnchanged(const std::vector<MemoryMatch<T>>& previous);

    // Read value at specific address
    template<typename T>
    bool readValue(uintptr_t address, T& outValue);

    // Write value at specific address
    template<typename T>
    bool writeValue(uintptr_t address, T value);

    // Read memory region
    bool readMemory(uintptr_t address, void* buffer, size_t size);

    // Write memory region
    bool writeMemory(uintptr_t address, const void* buffer, size_t size);

    // String-specific scan functions
    std::vector<MemoryMatch<std::string>> scanForString(const std::string& value);
    std::vector<MemoryMatch<std::wstring>> scanForWideString(const std::wstring& value);

private:
    HANDLE m_processHandle;
    std::vector<Module> m_modules;

    bool isReadableRegion(const MemoryRegion& region);
};

// Template implementations
template<typename T>
std::vector<MemoryMatch<T>> MemoryScanner::scanForValue(T value) {
    std::vector<MemoryMatch<T>> matches;
    auto regions = getReadableRegions();

    for (const auto& region : regions) {
        // Skip if region is too small
        if (region.size < sizeof(T)) continue;

        // Allocate buffer for this region
        std::vector<uint8_t> buffer(region.size);

        if (readMemory(region.baseAddress, buffer.data(), region.size)) {
            // Scan through the buffer
            for (size_t i = 0; i <= region.size - sizeof(T); i++) {
                // Use memcpy to avoid alignment issues
                T currentValue;
                std::memcpy(&currentValue, &buffer[i], sizeof(T));

                if (currentValue == value) {
                    MemoryMatch<T> match;
                    match.address = region.baseAddress + i;
                    match.value = currentValue;
                    matches.push_back(match);
                }
            }
        }
    }

    return matches;
}

template<typename T>
std::vector<MemoryMatch<T>> MemoryScanner::scanAllValues() {
    std::vector<MemoryMatch<T>> matches;
    auto regions = getReadableRegions();

    for (const auto& region : regions) {
        // Skip if region is too small
        if (region.size < sizeof(T)) continue;

        // Allocate buffer for this region
        std::vector<uint8_t> buffer(region.size);

        if (readMemory(region.baseAddress, buffer.data(), region.size)) {
            // Scan through the buffer
            for (size_t i = 0; i <= region.size - sizeof(T); i++) {
                // Use memcpy to avoid alignment issues
                T currentValue;
                std::memcpy(&currentValue, &buffer[i], sizeof(T));

                MemoryMatch<T> match;
                match.address = region.baseAddress + i;
                match.value = currentValue;
                matches.push_back(match);
            }
        }
    }

    return matches;
}

template<typename T>
std::vector<MemoryMatch<T>> MemoryScanner::filterByValue(const std::vector<MemoryMatch<T>>& previous, T value) {
    std::vector<MemoryMatch<T>> matches;

    for (const auto& match : previous) {
        T currentValue;
        if (readValue(match.address, currentValue) && currentValue == value) {
            MemoryMatch<T> newMatch;
            newMatch.address = match.address;
            newMatch.value = currentValue;
            matches.push_back(newMatch);
        }
    }

    return matches;
}

template<typename T>
std::vector<MemoryMatch<T>> MemoryScanner::filterByChanged(const std::vector<MemoryMatch<T>>& previous) {
    std::vector<MemoryMatch<T>> matches;

    for (const auto& match : previous) {
        T currentValue;
        if (readValue(match.address, currentValue) && currentValue != match.value) {
            MemoryMatch<T> newMatch;
            newMatch.address = match.address;
            newMatch.value = currentValue;
            matches.push_back(newMatch);
        }
    }

    return matches;
}

template<typename T>
std::vector<MemoryMatch<T>> MemoryScanner::filterByUnchanged(const std::vector<MemoryMatch<T>>& previous) {
    std::vector<MemoryMatch<T>> matches;

    for (const auto& match : previous) {
        T currentValue;
        if (readValue(match.address, currentValue) && currentValue == match.value) {
            MemoryMatch<T> newMatch;
            newMatch.address = match.address;
            newMatch.value = currentValue;
            matches.push_back(newMatch);
        }
    }

    return matches;
}

template<typename T>
bool MemoryScanner::readValue(uintptr_t address, T& outValue) {
    return readMemory(address, &outValue, sizeof(T));
}

template<typename T>
bool MemoryScanner::writeValue(uintptr_t address, T value) {
    return writeMemory(address, &value, sizeof(T));
}

#else
// Stub for non-Windows
class MemoryScanner {
public:
    explicit MemoryScanner(void*) {}
};
#endif
