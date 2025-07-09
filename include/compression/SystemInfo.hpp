#pragma once

#include <cstdint>

namespace compression {

/**
 * @brief Get the number of hardware threads available.
 */
uint32_t getHardwareThreads();

/**
 * @brief Get the amount of available physical memory in bytes.
 */
uint64_t getAvailableMemory();

} // namespace compression
