#include <compression/OptimizedCompressor.hpp>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

using namespace compression;

class OptimizedCompressorTest : public ::testing::Test {
protected:
  OptimizedCompressor compressor;
};

TEST_F(OptimizedCompressorTest, CompressEmptyData) {
  std::vector<uint8_t> data;
  auto compressed = compressor.compress(data);
  EXPECT_TRUE(compressed.empty());
}

TEST_F(OptimizedCompressorTest, DecompressEmptyData) {
  std::vector<uint8_t> data;
  auto decompressed = compressor.decompress(data);
  EXPECT_TRUE(decompressed.empty());
}

TEST_F(OptimizedCompressorTest, CompressRepetitiveData) {
  // This should trigger the repetitive data path (RLE-based)
  std::vector<uint8_t> data(1000, 0xAB);

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  EXPECT_EQ(data, decompressed);
  // Should achieve good compression on repetitive data
  EXPECT_LT(compressed.size(), data.size() / 2);
}

TEST_F(OptimizedCompressorTest, CompressPatternData) {
  // Should trigger the pattern data path (LZ77-based)
  std::string pattern = "ABCDEFGH";
  std::vector<uint8_t> data;

  // Repeat pattern 100 times
  for (int i = 0; i < 100; ++i) {
    data.insert(data.end(), pattern.begin(), pattern.end());
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  EXPECT_EQ(data, decompressed);
  // Should achieve compression on repeated patterns
  EXPECT_LT(compressed.size(), data.size());
}

TEST_F(OptimizedCompressorTest, CompressRandomData) {
  // Should trigger the entropy coding path
  std::vector<uint8_t> data;

  // Create pseudo-random but diverse data
  for (int i = 0; i < 1000; ++i) {
    data.push_back(static_cast<uint8_t>((i * 7 + 13) % 256));
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  EXPECT_EQ(data, decompressed);
}

TEST_F(OptimizedCompressorTest, RoundTripSmallData) {
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  EXPECT_EQ(data, decompressed);
}

TEST_F(OptimizedCompressorTest, RoundTripMixedData) {
  std::vector<uint8_t> data;

  // Mix of repetitive and varied data
  for (int i = 0; i < 100; ++i) {
    data.push_back(0xAA);
  }
  for (int i = 0; i < 100; ++i) {
    data.push_back(static_cast<uint8_t>(i));
  }
  for (int i = 0; i < 100; ++i) {
    data.push_back(0xBB);
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  EXPECT_EQ(data, decompressed);
}

TEST_F(OptimizedCompressorTest, HandleSingleByte) {
  std::vector<uint8_t> data = {0xFF};

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  EXPECT_EQ(data, decompressed);
}

TEST_F(OptimizedCompressorTest, HandleLongRuns) {
  std::vector<uint8_t> data(5000, 0x77);

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  EXPECT_EQ(data, decompressed);
  // Should compress very well
  EXPECT_LT(compressed.size(), data.size() / 10);
}

TEST_F(OptimizedCompressorTest, HandleTextData) {
  std::string text = "This is a test string with some repeated words. "
                     "Test string test string repeated words repeated words.";
  std::vector<uint8_t> data(text.begin(), text.end());

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  EXPECT_EQ(data, decompressed);
}

TEST_F(OptimizedCompressorTest, VerifyStrategySelection) {
  // Test that different data characteristics get handled appropriately

  // 1. Highly repetitive (low entropy)
  std::vector<uint8_t> repetitive(1000, 0x55);
  auto comp1 = compressor.compress(repetitive);
  auto decomp1 = compressor.decompress(comp1);
  EXPECT_EQ(repetitive, decomp1);

  // 2. Pattern-based
  std::vector<uint8_t> pattern;
  for (int i = 0; i < 100; ++i) {
    pattern.push_back('A');
    pattern.push_back('B');
    pattern.push_back('C');
    pattern.push_back('D');
  }
  auto comp2 = compressor.compress(pattern);
  auto decomp2 = compressor.decompress(comp2);
  EXPECT_EQ(pattern, decomp2);

  // 3. Higher entropy
  std::vector<uint8_t> diverse;
  for (int i = 0; i < 256; ++i) {
    diverse.push_back(static_cast<uint8_t>(i));
  }
  auto comp3 = compressor.compress(diverse);
  auto decomp3 = compressor.decompress(comp3);
  EXPECT_EQ(diverse, decomp3);
}

TEST_F(OptimizedCompressorTest, InvalidMethodByteRejected) {
  // A corrupt method byte must be rejected, not misrouted.
  auto data = std::vector<uint8_t>(100, 'A');
  auto compressed = compressor.compress(data);
  ASSERT_FALSE(compressed.empty());
  compressed[0] = 0x7F;
  EXPECT_THROW(compressor.decompress(compressed), std::runtime_error);
}

TEST_F(OptimizedCompressorTest, CorruptedPayloadRejected) {
  // Flip a byte mid-payload; decompression must throw or return different data.
  auto data = std::vector<uint8_t>(5000, 'x');
  auto compressed = compressor.compress(data);
  ASSERT_FALSE(compressed.empty());
  compressed[compressed.size() / 2] ^= 0x01;
  bool threw = false;
  std::vector<uint8_t> decompressed;
  try {
    decompressed = compressor.decompress(compressed);
  } catch (const std::exception &) {
    threw = true;
  }
  EXPECT_TRUE(threw || decompressed != data);
}
