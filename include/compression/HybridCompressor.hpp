#pragma once

#include <compression/ICompressor.hpp>
#include <memory>
#include <vector>
#include <cstdint>

namespace compression {

// Forward declarations
class Lz77Compressor;
class HuffmanCompressor;

/**
 * @struct DataAnalysis
 * @brief Analysis results for input data characteristics
 */
struct DataAnalysis {
    bool hasGoodLZ77Potential = false; // Data has good pattern matching potential
};

/**
 * @class HybridCompressor
 * @brief Intelligent hybrid compressor combining multiple techniques
 * 
 * This compressor analyzes input data and selects the optimal combination:
 * - LZ77+ method: LZ77 → RLE → Huffman (for data with good patterns)
 * - Huffman+ method: Preprocessing → Huffman (for other data)
 * 
 * Expected improvement: Better than individual algorithms by combining strengths
 */
class HybridCompressor : public ICompressor {
public:
    /**
     * @brief Constructor
     */
    HybridCompressor();
    
    /**
     * @brief Destructor
     */
    ~HybridCompressor();
    
    /**
     * @brief Compress data using optimal hybrid strategy
     * @param data Input data to compress
     * @return Compressed data with optimal ratio
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
    
    /**
     * @brief Decompress data from hybrid format
     * @param data Compressed data to decompress
     * @return Original decompressed data
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;

private:
    std::unique_ptr<Lz77Compressor> lz77_;
    std::unique_ptr<HuffmanCompressor> huffman_;
    
    // Data analysis
    DataAnalysis analyzeData(const std::vector<uint8_t>& data) const;
    
    // Compression strategies
    std::vector<uint8_t> compressWithLZ77Plus(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> compressWithHuffmanPlus(const std::vector<uint8_t>& data) const;
    
    // Decompression strategies
    std::vector<uint8_t> decompressWithLZ77Plus(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decompressWithHuffmanPlus(const std::vector<uint8_t>& data) const;
    
    // RLE processing for LZ77+
    std::vector<uint8_t> applyRLE(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> removeRLE(const std::vector<uint8_t>& data) const;
    
    // Preprocessing for Huffman+
    std::vector<uint8_t> preprocessForHuffman(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> postprocessFromHuffman(const std::vector<uint8_t>& data) const;
};

} // namespace compression
