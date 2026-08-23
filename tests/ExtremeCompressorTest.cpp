#include <compression/codec/legacy/ExtremeCompressor.hpp>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

using namespace compression;

class ExtremeCompressorTest : public ::testing::Test {
protected:
  ExtremeCompressor compressor;

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

TEST_F(ExtremeCompressorTest, EmptyData) {
  EXPECT_TRUE(compressor.compress({}).empty());
  EXPECT_TRUE(compressor.decompress({}).empty());
}

TEST_F(ExtremeCompressorTest, SmallDataRoundTrip) {
  auto data = textData(1000);
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(ExtremeCompressorTest, TextDataRoundTrip) {
  auto data = textData(30000);
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(ExtremeCompressorTest, TextCompresses) {
  auto data = textData(30000);
  auto compressed = compressor.compress(data);
  EXPECT_LT(compressed.size(), data.size());
}

TEST_F(ExtremeCompressorTest, RandomDataRoundTrip) {
  auto data = randomData(10000);
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(ExtremeCompressorTest, RepetitiveDataRoundTrip) {
  std::vector<uint8_t> data(30000, 'A');
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(ExtremeCompressorTest, InvalidMarkerRejected) {
  EXPECT_THROW(compressor.decompress({0x00, 0x01}), std::runtime_error);
}

TEST_F(ExtremeCompressorTest, CorruptedPayloadRejected) {
  auto data = textData(10000);
  auto compressed = compressor.compress(data);
  ASSERT_GE(compressed.size(), 2u);
  // Corrupt a byte in the middle of the payload (the trailing byte can hold
  // dead padding bits and flipping them is harmless).
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
