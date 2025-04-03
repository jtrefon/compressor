#pragma once

#include "ICompressor.hpp"

namespace compression {

/**
 * @brief Implements ICompressor using Run-Length Encoding (RLE).
 *
 * A simple RLE implementation.
 * Note: This basic version might increase size for non-repetitive data.
 */
class RleCompressor final : public ICompressor {
public:
    /**
     * @brief Compresses data using RLE.
     *
     * @param data The raw data to compress.
     * @return std::vector<std::byte> The RLE compressed data.
     */
    std::vector<std::byte> compress(const std::vector<std::byte>& data) const override;

    /**
     * @brief Decompresses RLE encoded data.
     *
     * @param data The RLE compressed data.
     * @return std::vector<std::byte> The original decompressed data.
     * @throws std::runtime_error if the compressed data format is invalid.
     */
    std::vector<std::byte> decompress(const std::vector<std::byte>& data) const override;
};

} // namespace compression 