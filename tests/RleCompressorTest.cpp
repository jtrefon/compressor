#include <compression/codec/legacy/RleCompressor.hpp>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

using namespace compression;

class RleCompressorTest : public ::testing::Test {
protected:
  RleCompressor compressor;
};

TEST_F(RleCompressorTest, CompressEmptyData) {
  std::vector<uint8_t> data;
  auto compressed = compressor.compress(data);
  EXPECT_TRUE(compressed.empty());
}

TEST_F(RleCompressorTest, DecompressEmptyData) {
  std::vector<uint8_t> data;
  auto decompressed = compressor.decompress(data);
  EXPECT_TRUE(decompressed.empty());
}

TEST_F(RleCompressorTest, CompressSingleByte) {
  std::vector<uint8_t> data = {0x42};
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(RleCompressorTest, CompressRepeatedSequence) {
  // RLE is optimal for repeated sequences
  std::vector<uint8_t> data(100, 0xAA); // 100 repetitions of 0xAA
  auto compressed = compressor.compress(data);

  // Compressed should be significantly smaller
  EXPECT_LT(compressed.size(), data.size());

  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(RleCompressorTest, CompressMixedData) {
  std::vector<uint8_t> data = {
      0x01, 0x01, 0x01, 0x01, // Run of 4
      0x02,                   // Single byte
      0x03, 0x03,             // Run of 2
      0x04, 0x05, 0x06        // No runs
  };

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(RleCompressorTest, CompressAlternatingBytes) {
  // Worst case for RLE - no repetitions
  std::vector<uint8_t> data;
  for (int i = 0; i < 100; ++i) {
    data.push_back(i % 2 == 0 ? 0x00 : 0xFF);
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(RleCompressorTest, CompressLongRun) {
  // Test run length limit
  std::vector<uint8_t> data(1000, 0x7F); // Very long run

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);

  // Should still achieve good compression
  EXPECT_LT(compressed.size(), data.size() / 2);
}

TEST_F(RleCompressorTest, RoundTripRandomData) {
  std::vector<uint8_t> data;
  for (int i = 0; i < 256; ++i) {
    data.push_back(static_cast<uint8_t>(i));
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(RleCompressorTest, CompressZeroBytes) {
  std::vector<uint8_t> data(50, 0x00); // Many zeros

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);

  // Should compress well
  EXPECT_LT(compressed.size(), data.size());
}

TEST_F(RleCompressorTest, CompressMaxByteValue) {
  std::vector<uint8_t> data(50, 0xFF); // Many 0xFF

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}
