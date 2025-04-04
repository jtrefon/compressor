#pragma once

#include "ICompressor.hpp"
#include <vector>
#include <cstdint>

namespace compression {

/**
 * @brief Implements ICompressor using Arithmetic coding.
 *
 * This class implements the compression algorithm using arithmetic coding,
 * which typically provides better compression ratios than Huffman coding.
 */
class ArithmeticCompressor final : public ICompressor {
public:
    /**
     * @brief Default constructor
     */
    ArithmeticCompressor() = default;
    
    /**
     * @brief Compresses the input data using arithmetic coding.
     *
     * @param data The raw data to compress.
     * @return std::vector<uint8_t> The compressed data.
     * @throws std::runtime_error on compression failure.
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
    
    /**
     * @brief Decompresses the input data using arithmetic coding.
     *
     * @param data The compressed data to decompress.
     * @return std::vector<uint8_t> The original decompressed data.
     * @throws std::runtime_error on decompression failure.
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;
};

} // namespace compression 