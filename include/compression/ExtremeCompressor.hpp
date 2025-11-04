#pragma once

#include <compression/ICompressor.hpp>
#include <memory>
#include <vector>
#include <cstdint>

namespace compression {

// Forward declarations
class BwtCompressor;
class HuffmanCompressor;
class Lz77Compressor;

/**
 * @class ExtremeCompressor
 * @brief Maximum compression ratio compressor using adaptive multi-strategy approach
 * 
 * This compressor tries multiple compression strategies and selects the best one:
 * - Strategy 1: BWT → LZ77 → Huffman
 * - Strategy 2: LZ77 → BWT → Huffman  
 * - Strategy 3: BWT → Huffman → LZ77
 * - Strategy 4: Double BWT → Huffman
 * - Strategy 5: Preprocess → BWT → LZ77 → Huffman
 * 
 * Features:
 * - Adaptive strategy selection based on data characteristics
 * - Multiple algorithm combinations for maximum compression
 * - Preprocessing transformations for better entropy
 * - Expected compression: Significantly better than any single algorithm
 * - Expected speed: Very slow (prioritizes maximum ratio above all else)
 */
class ExtremeCompressor : public ICompressor {
public:
    /**
     * @brief Constructor
     */
    ExtremeCompressor();
    
    /**
     * @brief Destructor
     */
    ~ExtremeCompressor();
    
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
    std::vector<uint8_t> applyBwtHuffmanLz77(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> applyDoubleBwtHuffman(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> applyPreprocessBwtLz77Huffman(const std::vector<uint8_t>& data) const;
    
    // Reverse implementations
    std::vector<uint8_t> reverseBwtLz77Huffman(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> reverseLz77BwtHuffman(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> reverseBwtHuffmanLz77(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> reverseDoubleBwtHuffman(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> reversePreprocessBwtLz77Huffman(const std::vector<uint8_t>& data) const;
    
    // Preprocessing for better compression
    std::vector<uint8_t> preprocessData(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> postprocessData(const std::vector<uint8_t>& data) const;

private:
    std::unique_ptr<BwtCompressor> bwt_;
    std::unique_ptr<HuffmanCompressor> huffman_;
    std::unique_ptr<Lz77Compressor> lz77_;
};

} // namespace compression
