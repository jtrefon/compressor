#include "compression/SystemInfo.hpp"

#include <thread>
#include <windows.h>

namespace compression {

uint32_t getHardwareThreads() {
    unsigned int n = std::thread::hardware_concurrency();
    return n ? n : 1;
}

uint64_t getAvailableMemory() {
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullAvailPhys;
    }
    return 0;
}

} // namespace compression
