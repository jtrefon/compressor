#pragma once

#include <compression/ICompressor.hpp>
#include <vector>
#include <cstdint>

namespace compression {

/**
 * @class ExtremeCompressor
 * @brief Maximum compression ratio compressor using adaptive multi-strategy approach
 *
 * This compressor tries multiple compression strategies and selects the best one:
 * - Strategy 0: BWT → LZ77 → Huffman
 * - Strategy 1: LZ77 → BWT → Huffman
 * - Strategy 2: Preprocess → BWT → LZ77 → Huffman
 * - Strategy 3: OptimizedCompressor
 * - Strategy 4: BwtCompressor
 *
 * Features:
 * - Adaptive strategy selection based on data characteristics
 * - Multiple algorithm combinations for maximum compression
 * - Preprocessing transformations for better entropy
 * - Expected compression: Significantly better than any single algorithm
 * - Expected speed: Very slow (prioritizes maximum ratio above all else)
 *
 * The strategies are evaluated concurrently on local compressor instances, so
 * a shared instance is safe to use from multiple threads.
 */
class ExtremeCompressor : public ICompressor {
public:
    ExtremeCompressor();

    /**
     * @brief Compress data using maximum compression ratio with adaptive strategy selection
     * @param data Input data to compress
     * @return Maximally compressed data using the best strategy
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;

    /**
     * @brief Decompress data from extreme-compressed format
     * @param data Extreme-compressed data to decompress
     * @return Original decompressed data
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;

private:
    // Strategy implementations
    std::vector<uint8_t> applyBwtLz77Huffman(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> applyLz77BwtHuffman(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> applyPreprocessBwtLz77Huffman(const std::vector<uint8_t>& data) const;

    // Reverse implementations
    std::vector<uint8_t> reverseBwtLz77Huffman(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> reverseLz77BwtHuffman(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> reversePreprocessBwtLz77Huffman(const std::vector<uint8_t>& data) const;

    // Preprocessing for better compression
    std::vector<uint8_t> preprocessData(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> postprocessData(const std::vector<uint8_t>& data) const;
};

} // namespace compression
