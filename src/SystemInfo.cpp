#include "compression/SystemInfo.hpp"

#include <thread>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <mach/mach.h>
#else
#include <sys/sysinfo.h>
#endif

namespace compression {

uint32_t getHardwareThreads() {
    unsigned int n = std::thread::hardware_concurrency();
    return n ? n : 1;
}

uint64_t getAvailableMemory() {
#if defined(_WIN32)
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullAvailPhys;
    }
    return 0;
#elif defined(__APPLE__)
    int64_t memsize = 0;
    size_t len = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &len, nullptr, 0) == 0) {
        mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
        vm_statistics64_data_t vmstat;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO,
                              reinterpret_cast<host_info64_t>(&vmstat),
                              &count) == KERN_SUCCESS) {
            uint64_t free = static_cast<uint64_t>(vmstat.free_count +
                                                  vmstat.inactive_count) *
                            vm_page_size;
            return free;
        }
    }
    return 0;
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return static_cast<uint64_t>(info.freeram) * info.mem_unit;
    }
    return 0;
#endif
}

} // namespace compression
