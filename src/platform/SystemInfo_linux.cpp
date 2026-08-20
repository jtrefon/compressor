#include "compression/SystemInfo.hpp"

#include <sys/sysinfo.h>
#include <thread>

namespace compression {

uint32_t getHardwareThreads() {
    unsigned int n = std::thread::hardware_concurrency();
    return n ? n : 1;
}

uint64_t getAvailableMemory() {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return static_cast<uint64_t>(info.freeram) * info.mem_unit;
    }
    return 0;
}

} // namespace compression
