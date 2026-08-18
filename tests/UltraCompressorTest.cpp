#include <compression/UltraCompressor.hpp>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

using namespace compression;

class UltraCompressorTest : public ::testing::Test {
protected:
  UltraCompressor compressor;

  std::vector<uint8_t> textData(size_t n) {
    std::vector<uint8_t> data;
    data.reserve(n);
    const std::string words[] = {"the", "quick", "brown", "fox", "jumps",
                                 "over", "lazy", "dog", "compression", "test"};
    size_t w = 0;
    while (data.size() < n) {
      for (char c : words[w % 10]) data.push_back(static_cast<uint8_t>(c));
      data.push_back(' ');
      w++;
    }
    data.resize(n);
    return data;
  }

  std::vector<uint8_t> randomData(size_t n) {
    std::vector<uint8_t> data(n);
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(0, 255);
    for (auto &b : data) b = static_cast<uint8_t>(dis(gen));
    return data;
  }
};

TEST_F(UltraCompressorTest, EmptyData) {
  EXPECT_TRUE(compressor.compress({}).empty());
  EXPECT_TRUE(compressor.decompress({}).empty());
}

TEST_F(UltraCompressorTest, SmallDataRoundTrip) {
  auto data = textData(1000);
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(UltraCompressorTest, TextDataRoundTrip) {
  auto data = textData(50000);
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(UltraCompressorTest, TextCompresses) {
  auto data = textData(50000);
  auto compressed = compressor.compress(data);
  EXPECT_LT(compressed.size(), data.size());
}

TEST_F(UltraCompressorTest, RandomDataRoundTrip) {
  auto data = randomData(20000);
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(UltraCompressorTest, RepetitiveDataRoundTrip) {
  std::vector<uint8_t> data(50000, 'A');
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(UltraCompressorTest, CorruptedPayloadRejected) {
  auto data = textData(20000);
  auto compressed = compressor.compress(data);
  ASSERT_FALSE(compressed.empty());
  // Corrupt a byte in the middle of the payload; decompression must either
  // throw or produce different output (never silently return the original).
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
