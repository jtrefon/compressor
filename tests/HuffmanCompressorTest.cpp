#include <compression/codec/legacy/HuffmanCompressor.hpp>
#include <cstdint>
#include <gtest/gtest.h>
#include <map>
#include <vector>

using namespace compression;

class HuffmanCompressorTest : public ::testing::Test {
protected:
  HuffmanCompressor compressor;
};

TEST_F(HuffmanCompressorTest, CompressEmptyData) {
  std::vector<uint8_t> data;
  auto compressed = compressor.compress(data);
  EXPECT_TRUE(compressed.empty());
}

TEST_F(HuffmanCompressorTest, DecompressEmptyData) {
  std::vector<uint8_t> data;
  auto decompressed = compressor.decompress(data);
  EXPECT_TRUE(decompressed.empty());
}

TEST_F(HuffmanCompressorTest, CompressSingleByte) {
  std::vector<uint8_t> data = {0x42};
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(HuffmanCompressorTest, CompressUniformDistribution) {
  //  All bytes appear equally - worst case for Huffman
  std::vector<uint8_t> data;
  for (int i = 0; i < 256; ++i) {
    data.push_back(static_cast<uint8_t>(i));
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(HuffmanCompressorTest, CompressSkewedDistribution) {
  // Huffman is optimal for skewed distributions
  std::vector<uint8_t> data;

  // 80% of data is 'A'
  for (int i = 0; i < 800; ++i) {
    data.push_back('A');
  }

  // 15% is 'B'
  for (int i = 0; i < 150; ++i) {
    data.push_back('B');
  }

  // 5% is mixed other characters
  for (int i = 0; i < 50; ++i) {
    data.push_back(static_cast<uint8_t>('C' + (i % 10)));
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  // Should achieve compression
  EXPECT_LT(compressed.size(), data.size());
  EXPECT_EQ(data, decompressed);
}

TEST_F(HuffmanCompressorTest, CompressRepeatedBytes) {
  std::vector<uint8_t> data(1000, 0x7F);

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  // Single byte repeated should compress very well
  EXPECT_LT(compressed.size(), data.size() / 2);
  EXPECT_EQ(data, decompressed);
}

TEST_F(HuffmanCompressorTest, CompressMixedData) {
  std::string text = "The quick brown fox jumps over the lazy dog. "
                     "Pack my box with five dozen liquor jugs. "
                     "How vexingly quick daft zebras jump!";
  std::vector<uint8_t> data(text.begin(), text.end());

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(HuffmanCompressorTest, RoundTripLargeData) {
  std::vector<uint8_t> data;

  // Create 10KB of data with varying frequencies
  for (int i = 0; i < 10000; ++i) {
    if (i % 3 == 0) {
      data.push_back('e'); // Most common
    } else if (i % 5 == 0) {
      data.push_back('t'); // Second most common
    } else {
      data.push_back(static_cast<uint8_t>(i % 26 + 'a'));
    }
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  EXPECT_EQ(data, decompressed);
  // Should achieve some compression on this distribution
  EXPECT_LT(compressed.size(), data.size());
}

TEST_F(HuffmanCompressorTest, HandleAllByteValues) {
  std::vector<uint8_t> data;

  // Include all possible byte values
  for (int i = 0; i < 256; ++i) {
    data.push_back(static_cast<uint8_t>(i));
  }

  // Add more of some values to create frequency variation
  for (int i = 0; i < 100; ++i) {
    data.push_back(0x00);
    data.push_back(0xFF);
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(HuffmanCompressorTest, VerifyCompressionRatioOnRepeatedPatterns) {
  std::vector<uint8_t> data;

  // Pattern where certain bytes are much more frequent
  for (int i = 0; i < 1000; ++i) {
    data.push_back(0x01); // 50%
    if (i % 2 == 0) {
      data.push_back(0x02); // 25%
    }
    if (i % 4 == 0) {
      data.push_back(0x03); // 12.5%
    }
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  EXPECT_EQ(data, decompressed);

  // Calculate compression ratio
  double ratio = static_cast<double>(compressed.size()) / data.size();

  // Should achieve better than 60% ratio on this highly skewed data
  EXPECT_LT(ratio, 0.6);
}

TEST_F(HuffmanCompressorTest, DecompressCorruptedHeader) {
  // Create valid compressed data first
  std::vector<uint8_t> data = {0x41, 0x42, 0x43};
  auto compressed = compressor.compress(data);

  // Corrupt the first byte (which contains size information)
  if (!compressed.empty()) {
    compressed[0] = 0xFF;

    // Decompression might throw or return garbage,
    // just verify it doesn't crash
    try {
      auto decompressed = compressor.decompress(compressed);
      // If it doesn't throw, that's okay too
    } catch (const std::exception &) {
      // Expected behavior for corrupted data
      SUCCEED();
    }
  }
}

TEST_F(HuffmanCompressorTest, CompressBinaryData) {
  std::vector<uint8_t> data;

  // Binary data with some structure
  for (int i = 0; i < 1000; ++i) {
    data.push_back(static_cast<uint8_t>(i & 0xFF));
    data.push_back(static_cast<uint8_t>((i >> 8) & 0xFF));
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}
