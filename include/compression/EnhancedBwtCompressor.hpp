#pragma once

#include <compression/ICompressor.hpp>
#include <memory>
#include <vector>
#include <cstdint>
#include <array>

namespace compression {

// Forward declaration
class ArithmeticCompressor;

/**
 * @class EnhancedBwtCompressor
 * @brief BWT compressor using arithmetic coding for improved compression ratio
 * 
 * This enhanced version of BWT replaces Huffman coding with arithmetic coding,
 * which can achieve compression ratios 5-15% closer to the theoretical entropy limit.
 * 
 * Pipeline: BWT → MTF → RLE → Arithmetic Coding
 */
class EnhancedBwtCompressor : public ICompressor {
public:
    /**
     * @brief Constructor
     */
    EnhancedBwtCompressor();
    
    /**
     * @brief Destructor
     */
    ~EnhancedBwtCompressor();
    
    /**
     * @brief Compress data using enhanced BWT with arithmetic coding
     * @param data Input data to compress
     * @return Compressed data with improved ratio
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
    
    /**
     * @brief Decompress data from enhanced BWT format
     * @param data Compressed data to decompress
     * @return Original decompressed data
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;

private:
    std::unique_ptr<ArithmeticCompressor> arithmetic_;
    
    // BWT transform methods
    std::vector<uint8_t> bwt_transform(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> bwt_inverse_transform(const std::vector<uint8_t>& data) const;
    
    // MTF encoding methods
    std::vector<uint8_t> mtf_encode(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> mtf_decode(const std::vector<uint8_t>& data) const;
    
    // RLE encoding methods
    std::vector<uint8_t> rle_encode(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> rle_decode(const std::vector<uint8_t>& data) const;
};

} // namespace compression
