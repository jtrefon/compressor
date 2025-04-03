#pragma once

#include <vector>
#include <cstddef> // For std::byte
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
     * @return std::vector<std::byte> The compressed data.
     * @throws std::runtime_error or derived class on compression failure.
     */
    virtual std::vector<std::byte> compress(const std::vector<std::byte>& data) const = 0;

    /**
     * @brief Decompresses the input data.
     *
     * Pure virtual function to be implemented by concrete strategies.
     *
     * @param data The compressed data to decompress.
     * @return std::vector<std::byte> The original decompressed data.
     * @throws std::runtime_error or derived class on decompression failure.
     */
    virtual std::vector<std::byte> decompress(const std::vector<std::byte>& data) const = 0;
};

} // namespace compression 