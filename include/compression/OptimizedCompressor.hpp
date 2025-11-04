#pragma once

#include <compression/ICompressor.hpp>
#include <memory>
#include <vector>
#include <cstdint>
#include <array>

namespace compression {

// Forward declarations
class Lz77Compressor;
class HuffmanCompressor;

/**
 * @struct DataCharacteristics
 * @brief Analysis results for input data characteristics
 */
struct DataCharacteristics {
    double entropy = 8.0;           // Data entropy (0-8 for bytes)
    bool isHighlyRepetitive = false; // Low entropy, high repetition
    bool hasLongRuns = false;        // Has long runs of identical bytes
    bool hasLongMatches = false;     // Has good pattern matching potential
};

/**
 * @class OptimizedCompressor
 * @brief Intelligent compressor that adapts strategy based on data characteristics
 * 
 * This compressor analyzes input data and selects the optimal compression strategy:
 * - Repetitive data: Enhanced RLE + Huffman
 * - Pattern data: Optimized LZ77 with larger window
 * - Entropy data: Preprocessing + Huffman
 * 
 * Expected improvement: 5-15% better compression than best single algorithm
 */
class OptimizedCompressor : public ICompressor {
public:
    /**
     * @brief Constructor
     */
    OptimizedCompressor();
    
    /**
     * @brief Destructor
     */
    ~OptimizedCompressor();
    
    /**
     * @brief Compress data using optimal strategy for data characteristics
     * @param data Input data to compress
     * @return Compressed data with optimal ratio
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
    
    /**
     * @brief Decompress data from optimized format
     * @param data Compressed data to decompress
     * @return Original decompressed data
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;

private:
    std::unique_ptr<Lz77Compressor> lz77_;
    std::unique_ptr<HuffmanCompressor> huffman_;
    
    // Data analysis
    DataCharacteristics analyzeData(const std::vector<uint8_t>& data) const;
    
    // Compression strategies
    std::vector<uint8_t> compressRepetitiveData(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> compressPatternData(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> compressEntropyData(const std::vector<uint8_t>& data) const;
    
    // Decompression strategies
    std::vector<uint8_t> decompressRepetitiveData(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decompressPatternData(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decompressEntropyData(const std::vector<uint8_t>& data) const;
    
    // Preprocessing for entropy compression
    std::vector<uint8_t> preprocessForEntropy(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> postprocessFromEntropy(const std::vector<uint8_t>& data) const;
};

} // namespace compression
