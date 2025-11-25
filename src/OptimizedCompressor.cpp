#include <algorithm>
#include <cmath>
#include <compression/HuffmanCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/OptimizedCompressor.hpp>
#include <map>
#include <string>

namespace compression {

OptimizedCompressor::OptimizedCompressor()
    : lz77_(std::make_unique<Lz77Compressor>(65536, 3, 258, false, true, true)),
      huffman_(std::make_unique<HuffmanCompressor>()) {}

OptimizedCompressor::~OptimizedCompressor() = default;

std::vector<uint8_t>
OptimizedCompressor::compress(const std::vector<uint8_t> &data) const {
  if (data.empty()) {
    return {};
  }

  // Analyze data characteristics to choose optimal strategy
  DataCharacteristics characteristics = analyzeData(data);

  if (characteristics.isHighlyRepetitive) {
    // Use enhanced BWT-like approach for repetitive data
    return compressRepetitiveData(data);
  } else if (characteristics.hasLongMatches) {
    // Use optimized LZ77 for data with good pattern matching
    return compressPatternData(data);
  } else {
    // Use entropy coding for diverse data
    return compressEntropyData(data);
  }
}

std::vector<uint8_t>
OptimizedCompressor::decompress(const std::vector<uint8_t> &data) const {
  if (data.empty()) {
    return {};
  }

  try {
    // Detect compression method from header
    if (data.size() < 1) {
      throw std::runtime_error("Invalid compressed data: too short");
    }

    uint8_t method = data[0];
    const std::vector<uint8_t> compressedData(data.begin() + 1, data.end());

    switch (method) {
    case 0x01: // Repetitive data method
      return decompressRepetitiveData(compressedData);
    case 0x02: // Pattern data method
      return decompressPatternData(compressedData);
    case 0x03: // Entropy data method
      return decompressEntropyData(compressedData);
    default:
      throw std::runtime_error("Unknown compression method: " +
                               std::to_string(static_cast<int>(method)));
    }
  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("Optimized decompression failed: ") +
                             e.what());
  }
}

DataCharacteristics
OptimizedCompressor::analyzeData(const std::vector<uint8_t> &data) const {
  DataCharacteristics characteristics;

  if (data.size() < 100) {
    characteristics.entropy = 8.0;
    return characteristics; // Too small for meaningful analysis
  }

  // Calculate entropy
  std::array<size_t, 256> freq{};
  for (uint8_t byte : data) {
    freq[byte]++;
  }

  double entropy = 0.0;
  for (size_t count : freq) {
    if (count > 0) {
      double p = static_cast<double>(count) / data.size();
      entropy -= p * std::log2(p);
    }
  }

  characteristics.entropy = entropy;
  characteristics.isHighlyRepetitive = (entropy < 6.0);

  // Check for long repeated sequences
  size_t maxRun = 1;
  size_t currentRun = 1;
  for (size_t i = 1; i < data.size(); ++i) {
    if (data[i] == data[i - 1]) {
      currentRun++;
    } else {
      maxRun = std::max(maxRun, currentRun);
      currentRun = 1;
    }
  }
  maxRun = std::max(maxRun, currentRun);

  characteristics.hasLongRuns = (maxRun > 10);

  // Check for pattern matches (simplified)
  std::map<std::vector<uint8_t>, size_t> patterns;
  const size_t patternLength = 4;

  for (size_t i = 0; i + patternLength <= data.size(); ++i) {
    std::vector<uint8_t> pattern(data.begin() + i,
                                 data.begin() + i + patternLength);
    patterns[pattern]++;
  }

  size_t repeatedPatterns = 0;
  for (const auto &[pattern, count] : patterns) {
    if (count > 1) {
      repeatedPatterns++;
    }
  }

  characteristics.hasLongMatches = (repeatedPatterns > patterns.size() * 0.1);

  return characteristics;
}

std::vector<uint8_t> OptimizedCompressor::compressRepetitiveData(
    const std::vector<uint8_t> &data) const {
  // Enhanced RLE for highly repetitive data
  std::vector<uint8_t> rleData;

  size_t i = 0;
  while (i < data.size()) {
    uint8_t current = data[i];
    size_t count = 1;

    // Count consecutive identical bytes
    while (i + count < data.size() && data[i + count] == current &&
           count < 65535) {
      count++;
    }

    if (count >= 3 || current == 0) {
      // Encode as run: [0xFF][count_low][count_high][value]
      rleData.push_back(0xFF);
      rleData.push_back(static_cast<uint8_t>(count & 0xFF));
      rleData.push_back(static_cast<uint8_t>((count >> 8) & 0xFF));
      rleData.push_back(current);
    } else {
      // Encode as literal bytes
      rleData.push_back(current);
    }

    i += count;
  }

  // Apply Huffman compression to the RLE output
  auto huffmanCompressed = huffman_->compress(rleData);

  // Add method marker at the beginning
  std::vector<uint8_t> result;
  result.push_back(0x01);
  result.insert(result.end(), huffmanCompressed.begin(),
                huffmanCompressed.end());

  return result;
}

std::vector<uint8_t> OptimizedCompressor::compressPatternData(
    const std::vector<uint8_t> &data) const {
  // Optimized LZ77 with larger window and better match finding
  auto lz77Compressed = lz77_->compress(data);

  // Add method marker at the beginning
  std::vector<uint8_t> result;
  result.push_back(0x02);
  result.insert(result.end(), lz77Compressed.begin(), lz77Compressed.end());

  return result;
}

std::vector<uint8_t> OptimizedCompressor::compressEntropyData(
    const std::vector<uint8_t> &data) const {
  // Enhanced entropy coding with preprocessing

  // Apply simple byte transformation to improve entropy
  std::vector<uint8_t> transformed = preprocessForEntropy(data);

  auto huffmanCompressed = huffman_->compress(transformed);

  // Add method marker at the beginning
  std::vector<uint8_t> result;
  result.push_back(0x03);
  result.insert(result.end(), huffmanCompressed.begin(),
                huffmanCompressed.end());

  return result;
}

std::vector<uint8_t> OptimizedCompressor::decompressRepetitiveData(
    const std::vector<uint8_t> &data) const {
  // First decompress Huffman
  auto huffmanDecompressed = huffman_->decompress(data);

  // Then decompress enhanced RLE
  std::vector<uint8_t> result;

  size_t i = 0;
  while (i < huffmanDecompressed.size()) {
    uint8_t current = huffmanDecompressed[i++];

    if (current == 0xFF && i + 2 < huffmanDecompressed.size()) {
      // Run-length encoding
      size_t count = static_cast<size_t>(huffmanDecompressed[i]) |
                     (static_cast<size_t>(huffmanDecompressed[i + 1]) << 8);
      uint8_t value = huffmanDecompressed[i + 2];
      i += 3;

      result.insert(result.end(), count, value);
    } else {
      // Literal byte
      result.push_back(current);
    }
  }

  return result;
}

std::vector<uint8_t> OptimizedCompressor::decompressPatternData(
    const std::vector<uint8_t> &data) const {
  return lz77_->decompress(data);
}

std::vector<uint8_t> OptimizedCompressor::decompressEntropyData(
    const std::vector<uint8_t> &data) const {
  auto huffmanDecompressed = huffman_->decompress(data);
  return postprocessFromEntropy(huffmanDecompressed);
}

std::vector<uint8_t> OptimizedCompressor::preprocessForEntropy(
    const std::vector<uint8_t> &data) const {
  // Simple XOR-based preprocessing to improve entropy
  std::vector<uint8_t> transformed;
  transformed.reserve(data.size());

  if (data.empty()) {
    return transformed;
  }

  transformed.push_back(data[0]);
  for (size_t i = 1; i < data.size(); ++i) {
    transformed.push_back(data[i] ^ data[i - 1]);
  }

  return transformed;
}

std::vector<uint8_t> OptimizedCompressor::postprocessFromEntropy(
    const std::vector<uint8_t> &data) const {
  // Reverse the XOR preprocessing
  std::vector<uint8_t> result;
  result.reserve(data.size());

  if (data.empty()) {
    return result;
  }

  result.push_back(data[0]);
  for (size_t i = 1; i < data.size(); ++i) {
    result.push_back(data[i] ^ result[i - 1]);
  }

  return result;
}

} // namespace compression
