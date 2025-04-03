#pragma once

#include <cstdint>
#include <vector>
#include <cstddef> // std::byte
#include <array>

namespace compression {
namespace utils {

/**
 * @brief Simple table-based CRC32 implementation.
 */
class Crc32 {
private:
    std::array<uint32_t, 256> crc_table;
    static constexpr uint32_t POLYNOMIAL = 0xEDB88320; // Standard CRC32 polynomial (reversed)

    void generateTable() {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (size_t j = 0; j < 8; ++j) {
                if (c & 1) {
                    c = POLYNOMIAL ^ (c >> 1);
                } else {
                    c >>= 1;
                }
            }
            crc_table[i] = c;
        }
    }

public:
    Crc32() {
        generateTable();
    }

    /**
     * @brief Calculates the CRC32 checksum for a block of data.
     *
     * @param data Pointer to the data buffer.
     * @param size Size of the data buffer in bytes.
     * @return The calculated CRC32 checksum.
     */
    uint32_t calculate(const std::byte* data, size_t size) const {
        uint32_t crc = 0xFFFFFFFF; // Initial value
        for (size_t i = 0; i < size; ++i) {
            crc = crc_table[(crc ^ static_cast<uint8_t>(data[i])) & 0xFF] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFF; // Final XOR value
    }

    /**
     * @brief Calculates the CRC32 checksum for a vector of bytes.
     *
     * @param data The vector of bytes.
     * @return The calculated CRC32 checksum.
     */
    uint32_t calculate(const std::vector<std::byte>& data) const {
        return calculate(data.data(), data.size());
    }
};

// Static instance for easy use
inline const Crc32 crc32Calculator; 

} // namespace utils
} // namespace compression 