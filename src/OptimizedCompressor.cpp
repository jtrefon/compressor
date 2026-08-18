#include <algorithm>
#include <cmath>
#include <compression/BwtCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/OptimizedCompressor.hpp>
#include <map>
#include <string>

namespace compression {

OptimizedCompressor::OptimizedCompressor()
    : lz77_(std::make_unique<Lz77Compressor>(65536, 3, 258, false, true, true)),
      huffman_(std::make_unique<HuffmanCompressor>()),
      bwt_(std::make_unique<BwtCompressor>()) {}

OptimizedCompressor::~OptimizedCompressor() = default;

std::vector<uint8_t>
OptimizedCompressor::compress(const std::vector<uint8_t> &data) const {
  if (data.empty()) {
    return {};
  }

  // Try all strategies and pick the best one
  std::vector<uint8_t> bestResult;

  // Always try Store mode as baseline (1 byte overhead)
  size_t bestSize = data.size() + 1;

  // Helper to update best result
  auto updateBest = [&](const std::vector<uint8_t> &candidate) {
    if (!candidate.empty() && candidate.size() < bestSize) {
      bestResult = candidate;
      bestSize = candidate.size();
    }
  };

  // Try Repetitive (RLE + Huffman)
  updateBest(compressRepetitiveData(data));

  // Try Pattern (LZ77)
  updateBest(compressPatternData(data));

  // Try Entropy (XOR + Huffman)
  updateBest(compressEntropyData(data));

  // Try BWT (BWT + MTF + ZRL + Huffman)
  updateBest(compressBwtData(data));

  // Try Deflate (LZ77 + Huffman)
  updateBest(compressDeflateData(data));

  auto huffmanCompressed = huffman_->compress(data);
  std::vector<uint8_t> huffmanResult;
  huffmanResult.push_back(0x06);
  huffmanResult.insert(huffmanResult.end(), huffmanCompressed.begin(),
                       huffmanCompressed.end());
  updateBest(huffmanResult);

  // If the best we found is worse than storing, fallback to Store Mode.
  if (bestResult.empty() || bestResult.size() >= data.size() + 1) {
    // Store mode: [0x00] [Original Data]
    std::vector<uint8_t> storeResult;
    storeResult.reserve(data.size() + 1);
    storeResult.push_back(0x00);
    storeResult.insert(storeResult.end(), data.begin(), data.end());
    return storeResult;
  }

  return bestResult;
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
    case 0x00: // Store mode (no compression)
      return compressedData;
    case 0x01: // Repetitive data method
      return decompressRepetitiveData(compressedData);
    case 0x02: // Pattern data method
      return decompressPatternData(compressedData);
    case 0x03: // Entropy data method
      return decompressEntropyData(compressedData);
    case 0x04: // BWT method
      return decompressBwtData(compressedData);
    case 0x05: // Deflate method (LZ77 + Huffman)
      return decompressDeflateData(compressedData);
    case 0x06:
      return huffman_->decompress(compressedData);
    default:
      throw std::runtime_error("Unknown compression method: " +
                               std::to_string(static_cast<int>(method)));
    }
  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("Optimized decompression failed: ") +
                             e.what());
  }
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
    } else if (current == 0xFF) {
      // Escape a lone 0xFF literal so the decoder does not mistake it for a
      // run header: [0xFF][0x00][0x00][value] (count 0 marks an escape)
      rleData.push_back(0xFF);
      rleData.push_back(0x00);
      rleData.push_back(0x00);
      rleData.push_back(0xFF);
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

std::vector<uint8_t>
OptimizedCompressor::compressBwtData(const std::vector<uint8_t> &data) const {
  // BWT compression (BWT -> MTF -> ZRL -> Huffman)
  auto bwtCompressed = bwt_->compress(data);

  // Add method marker at the beginning
  std::vector<uint8_t> result;
  result.push_back(0x04);
  result.insert(result.end(), bwtCompressed.begin(), bwtCompressed.end());

  return result;
}

std::vector<uint8_t> OptimizedCompressor::compressDeflateData(
    const std::vector<uint8_t> &data) const {
  // Deflate-style: LZ77 followed by Huffman
  auto lz77Compressed = lz77_->compress(data);
  auto huffmanCompressed = huffman_->compress(lz77Compressed);

  // Add method marker at the beginning
  std::vector<uint8_t> result;
  result.push_back(0x05);
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

      if (count == 0) {
        // Escaped literal 0xFF
        result.push_back(value);
      } else {
        result.insert(result.end(), count, value);
      }
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

std::vector<uint8_t>
OptimizedCompressor::decompressBwtData(const std::vector<uint8_t> &data) const {
  return bwt_->decompress(data);
}

std::vector<uint8_t> OptimizedCompressor::decompressDeflateData(
    const std::vector<uint8_t> &data) const {
  auto huffmanDecompressed = huffman_->decompress(data);
  return lz77_->decompress(huffmanDecompressed);
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
