#include <compression/codec/legacy/ArithmeticCompressor.hpp>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

using namespace compression;

class ArithmeticCompressorTest : public ::testing::Test {
protected:
  ArithmeticCompressor compressor;

  std::vector<uint8_t> stringToBytes(const std::string &s) {
    return std::vector<uint8_t>(s.begin(), s.end());
  }

  std::string bytesToString(const std::vector<uint8_t> &b) {
    return std::string(b.begin(), b.end());
  }
};

TEST_F(ArithmeticCompressorTest, EmptyData) {
  std::vector<uint8_t> data;
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(ArithmeticCompressorTest, SimpleString) {
  std::string s = "Hello Arithmetic Coding!";
  auto data = stringToBytes(s);
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(s, bytesToString(decompressed));
}

TEST_F(ArithmeticCompressorTest, RepeatedPattern) {
  std::vector<uint8_t> data(100, 'A');
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
  EXPECT_LT(compressed.size(), data.size()); // Should compress well
}

TEST_F(ArithmeticCompressorTest, MediumRandomData) {
  std::vector<uint8_t> data;
  // Generate 10KB of random data
  std::mt19937 gen(42);
  std::uniform_int_distribution<> dis(0, 255);
  for (int i = 0; i < 10000; ++i) {
    data.push_back(static_cast<uint8_t>(dis(gen)));
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  // Check size matches first
  ASSERT_EQ(data.size(), decompressed.size());
  EXPECT_EQ(data, decompressed);
}

TEST_F(ArithmeticCompressorTest, SkewedData) {
  std::vector<uint8_t> data;
  // Generate data dominated by '0' (like MTF)
  std::mt19937 gen(42);
  std::uniform_int_distribution<> dis(0, 255);
  std::uniform_real_distribution<> prob(0.0, 1.0);

  for (int i = 0; i < 10000; ++i) {
    if (prob(gen) < 0.9) {
      data.push_back(0);
    } else {
      data.push_back(static_cast<uint8_t>(dis(gen)));
    }
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  ASSERT_EQ(data.size(), decompressed.size());
  EXPECT_EQ(data, decompressed);
  // Should compress very well
  EXPECT_LT(compressed.size(), data.size() / 2);
}

TEST_F(ArithmeticCompressorTest, LargeData) {
  std::vector<uint8_t> data;
  // Generate 7MB of data to mimic benchmark
  size_t size = 7 * 1024 * 1024;
  data.reserve(size);

  std::mt19937 gen(12345);
  std::uniform_int_distribution<> dis(0, 255);

  for (size_t i = 0; i < size; ++i) {
    data.push_back(static_cast<uint8_t>(dis(gen)));
  }

  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);

  ASSERT_EQ(data.size(), decompressed.size());
  // Check content (checking start, middle, end to save time/memory if eq
  // expensive)
  for (size_t i = 0; i < size; i += 1024) {
    ASSERT_EQ(data[i], decompressed[i]) << "Mismatch at index " << i;
  }
  if (data != decompressed) {
    FAIL() << "Data mismatch full check";
  }
}

TEST_F(ArithmeticCompressorTest, All256Symbols) {
  // Regression guard: all 256 symbols in one stream (entry-count header
  // overflow).
  std::vector<uint8_t> data;
  data.reserve(512);
  for (int r = 0; r < 2; ++r) {
    for (int i = 0; i < 256; ++i) data.push_back(static_cast<uint8_t>(i));
  }
  auto compressed = compressor.compress(data);
  auto decompressed = compressor.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(ArithmeticCompressorTest, CorruptedPayloadRejected) {
  std::vector<uint8_t> data(1000, 'x');
  auto compressed = compressor.compress(data);
  ASSERT_FALSE(compressed.empty());
  compressed[compressed.size() / 2] ^= 0x01;
  EXPECT_THROW(compressor.decompress(compressed), std::runtime_error);
}

TEST_F(ArithmeticCompressorTest, TruncatedHeaderRejected) {
  EXPECT_THROW(compressor.decompress({0x01, 0x02, 0x03}), std::runtime_error);
}
