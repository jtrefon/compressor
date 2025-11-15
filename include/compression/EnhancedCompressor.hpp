#pragma once

#include <compression/ICompressor.hpp>
#include <memory>
#include <vector>
#include <cstdint>
#include <array>

namespace compression {

// Forward declarations
class BwtCompressor;
class Lz77Compressor;
class HuffmanCompressor;

/**
 * @class EnhancedCompressor
 * @brief Multi-stage compression combining the best algorithms for maximum ratio
 * 
 * This compressor uses a sophisticated pipeline:
 * 1. BWT transform (when beneficial) - excellent for repetitive/structured data
 * 2. LZ77 compression - handles dictionary-based patterns
 * 3. Huffman coding - final entropy compression
 * 
 * The compressor automatically detects when to use BWT based on data characteristics,
 * providing optimal compression across different data types while maintaining
 * reasonable performance.
 */
class EnhancedCompressor : public ICompressor {
public:
    /**
     * @brief Constructor - initializes the compression pipeline
     */
    EnhancedCompressor();
    
    /**
     * @brief Destructor
     */
    ~EnhancedCompressor();
    
    /**
     * @brief Compress data using the enhanced multi-stage algorithm
     * @param data Input data to compress
     * @return Compressed data with maximum compression ratio
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
    
    /**
     * @brief Decompress data from the enhanced multi-stage format
     * @param data Compressed data to decompress
     * @return Original decompressed data
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;

private:
    std::unique_ptr<BwtCompressor> bwt_;
    std::unique_ptr<Lz77Compressor> lz77_;
    std::unique_ptr<HuffmanCompressor> huffman_;
    
    /**
     * @brief Determine if BWT would be beneficial for this data
     * @param data Input data to analyze
     * @return true if BWT should be used
     */
    bool shouldUseBWT(const std::vector<uint8_t>& data) const;
    
    /**
     * @brief Detect if BWT was used during compression
     * @param data Decompressed data to analyze
     * @return true if BWT was likely used
     */
    bool detectBWTUsage(const std::vector<uint8_t>& data) const;
};

} // namespace compression
