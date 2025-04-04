#pragma once

#include <vector>
#include <cstdint> // For uint8_t
#include <stdexcept> // For potential exceptions

namespace compression {

/**
 * @brief Interface for compression and decompression algorithms.
 *
 * This abstract class defines the contract for different compression
 * strategies, adhering to the Strategy design pattern.
 */
class ICompressor {
public:
    /**
     * @brief Virtual destructor. Essential for polymorphic behavior.
     */
    virtual ~ICompressor() = default;

    /**
     * @brief Compresses the input data.
     *
     * Pure virtual function to be implemented by concrete strategies.
     *
     * @param data The raw data to compress.
     * @return std::vector<uint8_t> The compressed data.
     * @throws std::runtime_error or derived class on compression failure.
     */
    virtual std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const = 0;

    /**
     * @brief Decompresses the input data.
     *
     * Pure virtual function to be implemented by concrete strategies.
     *
     * @param data The compressed data to decompress.
     * @return std::vector<uint8_t> The original decompressed data.
     * @throws std::runtime_error or derived class on decompression failure.
     */
    virtual std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const = 0;
};

} // namespace compression 