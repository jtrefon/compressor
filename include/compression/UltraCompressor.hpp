#pragma once

#include <compression/ICompressor.hpp>
#include <memory>
#include <vector>
#include <cstdint>

namespace compression {

// Forward declarations
class BwtCompressor;
class Lz77Compressor;

/**
 * @class UltraCompressor
 * @brief Maximum compression ratio compressor using multi-stage approach
 * 
 * This compressor combines multiple techniques for maximum compression:
 * - Stage 1: BWT transform (excellent for repetitive data)
 * - Stage 2: LZ77 compression on BWT output
 * 
 * Expected compression: Better than any individual algorithm
 * Expected speed: Slow (prioritizes ratio over speed)
 */
class UltraCompressor : public ICompressor {
public:
    /**
     * @brief Constructor
     */
    UltraCompressor();
    
    /**
     * @brief Destructor
     */
    ~UltraCompressor();
    
    /**
     * @brief Compress data using maximum compression ratio
     * @param data Input data to compress
     * @return Maximally compressed data
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
    
    /**
     * @brief Decompress data from ultra-compressed format
     * @param data Ultra-compressed data to decompress
     * @return Original decompressed data
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;

private:
    std::unique_ptr<BwtCompressor> bwt_;
    std::unique_ptr<Lz77Compressor> lz77_;
};

} // namespace compression
