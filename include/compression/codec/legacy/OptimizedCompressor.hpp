#pragma once

#include <array>
#include <compression/ICompressor.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace compression {

// Forward declarations
class Lz77Compressor;
class HuffmanCompressor;
class BwtCompressor;

/**
 * @class OptimizedCompressor
 * @brief Intelligent compressor that adapts strategy based on data
 * characteristics
 *
 * This compressor analyzes input data and selects the optimal compression
 * strategy:
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
  std::vector<uint8_t>
  compress(const std::vector<uint8_t> &data) const override;

  /**
   * @brief Decompress data from optimized format
   * @param data Compressed data to decompress
   * @return Original decompressed data
   */
  std::vector<uint8_t>
  decompress(const std::vector<uint8_t> &data) const override;

private:
  std::unique_ptr<Lz77Compressor> lz77_;
  std::unique_ptr<HuffmanCompressor> huffman_;
  std::unique_ptr<BwtCompressor> bwt_;

  // Compression strategies
  std::vector<uint8_t>
  compressRepetitiveData(const std::vector<uint8_t> &data) const;
  std::vector<uint8_t>
  compressPatternData(const std::vector<uint8_t> &data) const;
  std::vector<uint8_t>
  compressEntropyData(const std::vector<uint8_t> &data) const;
  std::vector<uint8_t> compressBwtData(const std::vector<uint8_t> &data) const;
  std::vector<uint8_t>
  compressDeflateData(const std::vector<uint8_t> &data) const;

  // Decompression strategies
  std::vector<uint8_t>
  decompressRepetitiveData(const std::vector<uint8_t> &data) const;
  std::vector<uint8_t>
  decompressPatternData(const std::vector<uint8_t> &data) const;
  std::vector<uint8_t>
  decompressEntropyData(const std::vector<uint8_t> &data) const;
  std::vector<uint8_t>
  decompressBwtData(const std::vector<uint8_t> &data) const;
  std::vector<uint8_t>
  decompressDeflateData(const std::vector<uint8_t> &data) const;

  // Preprocessing for entropy compression
  std::vector<uint8_t>
  preprocessForEntropy(const std::vector<uint8_t> &data) const;
  std::vector<uint8_t>
  postprocessFromEntropy(const std::vector<uint8_t> &data) const;
};

} // namespace compression
