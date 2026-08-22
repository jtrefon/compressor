#include "compression/SystemInfo.hpp"

#include <mach/mach.h>
#include <sys/sysctl.h>
#include <thread>

namespace compression {

uint32_t getHardwareThreads() {
    unsigned int n = std::thread::hardware_concurrency();
    return n ? n : 1;
}

uint64_t getAvailableMemory() {
    int64_t memsize = 0;
    size_t len = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &len, nullptr, 0) != 0) {
        return 0;
    }
    mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
    vm_statistics64_data_t vmstat;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO,
                          reinterpret_cast<host_info64_t>(&vmstat),
                          &count) != KERN_SUCCESS) {
        return 0;
    }
    uint64_t free = static_cast<uint64_t>(vmstat.free_count +
                                          vmstat.inactive_count) *
                    vm_page_size;
    return free;
}

} // namespace compression
