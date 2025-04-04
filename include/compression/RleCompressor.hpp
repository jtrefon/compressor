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
     * @return std::vector<uint8_t> The RLE compressed data.
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;

    /**
     * @brief Decompresses RLE encoded data.
     *
     * @param data The RLE compressed data.
     * @return std::vector<uint8_t> The original decompressed data.
     * @throws std::runtime_error if the compressed data format is invalid.
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;
};

} // namespace compression 