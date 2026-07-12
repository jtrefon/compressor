#include <compression/ArithmeticCompressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/ExtremeCompressor.hpp>
#include <compression/OptimizedCompressor.hpp>
#include <gtest/gtest.h>
#include <random>
#include <vector>

using namespace compression;

class BugRegressionTest : public ::testing::Test {
};

// Bug fix 1: BWT periodic data — without EOF sentinel, primary_index can be wrong
TEST_F(BugRegressionTest, BwtPeriodicData) {
  BwtCompressor bwt;
  // "ABABAB" has equivalent rotations — fails without sentinel
  std::vector<uint8_t> data = {'A', 'B', 'A', 'B', 'A', 'B'};
  auto compressed = bwt.compress(data);
  auto decompressed = bwt.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(BugRegressionTest, BwtAllSameByte) {
  BwtCompressor bwt;
  // All identical bytes — fails without sentinel
  std::vector<uint8_t> data(100, 0x42);
  auto compressed = bwt.compress(data);
  auto decompressed = bwt.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(BugRegressionTest, BwtBinaryWithNull) {
  BwtCompressor bwt;
  // Data containing 0x00 and 0xFF — tests escaping
  std::vector<uint8_t> data = {0x00, 0x41, 0xFF, 0x42, 0x00, 0xFF};
  auto compressed = bwt.compress(data);
  auto decompressed = bwt.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

// Bug fix 2: Arithmetic coder carry propagation — 0x00 vs 0xFF
TEST_F(BugRegressionTest, ArithmeticCarryPropagation) {
  ArithmeticCompressor ac;
  // Highly repetitive data triggers carries in the range coder
  // 1000 identical bytes force the range to shrink dramatically
  std::vector<uint8_t> data(1000, 'A');
  auto compressed = ac.compress(data);
  auto decompressed = ac.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST_F(BugRegressionTest, ArithmeticAlternatingBytes) {
  ArithmeticCompressor ac;
  // Alternating pattern also triggers carries
  std::vector<uint8_t> data;
  for (int i = 0; i < 500; i++) {
    data.push_back('A');
    data.push_back('B');
  }
  auto compressed = ac.compress(data);
  auto decompressed = ac.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

// Bug fix 3: LZ77 distance limit hardcoded to 32768
TEST_F(BugRegressionTest, Lz77LargeDistance) {
  // With window 65536, distances beyond 32768 should be found
  Lz77Compressor lz77(65536, 3, 258, false, true, true);
  
  // Create data where a match exists at distance > 32768
  std::vector<uint8_t> data(70000);
  // Fill with pattern at position 0
  for (int i = 0; i < 100; i++) {
    data[i] = static_cast<uint8_t>('A' + (i % 26));
  }
  // Fill gap
  for (int i = 100; i < 40000; i++) {
    data[i] = static_cast<uint8_t>(i % 256);
  }
  // Repeat pattern at position 40000 (distance from 0 = 40000 > 32768)
  for (int i = 0; i < 100; i++) {
    data[40000 + i] = static_cast<uint8_t>('A' + (i % 26));
  }
  // Fill rest
  for (int i = 40100; i < 70000; i++) {
    data[i] = static_cast<uint8_t>(i % 256);
  }
  
  auto compressed = lz77.compress(data);
  auto decompressed = lz77.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

// Bug fix 4: ExtremeCompressor double-BWT was broken
TEST_F(BugRegressionTest, ExtremeCompressorRoundTrip) {
  ExtremeCompressor ec;
  // Various data types
  std::vector<uint8_t> text(1000, 'A');
  for (int i = 0; i < 1000; i++) {
    text[i] = static_cast<uint8_t>('A' + (i % 26));
  }
  auto compressed = ec.compress(text);
  auto decompressed = ec.decompress(compressed);
  EXPECT_EQ(text, decompressed);
  
  std::vector<uint8_t> random(500);
  std::mt19937 gen(42);
  std::uniform_int_distribution<> dis(0, 255);
  for (int i = 0; i < 500; i++) {
    random[i] = static_cast<uint8_t>(dis(gen));
  }
  compressed = ec.compress(random);
  decompressed = ec.decompress(compressed);
  EXPECT_EQ(random, decompressed);
}

// Bug fix 5: OptimizedCompressor RLE 0xFF bug
TEST_F(BugRegressionTest, OptimizedRleSingle0xFF) {
  // Data with isolated 0xFF — previously encoded as 4-byte run
  std::vector<uint8_t> data = {0x41, 0xFF, 0x42};
  auto optimized = std::make_unique<OptimizedCompressor>();
  auto compressed = optimized->compress(data);
  auto decompressed = optimized->decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

// Bug fix 6: BWT with all 0xFF bytes
TEST_F(BugRegressionTest, BwtAll0xFF) {
  BwtCompressor bwt;
  std::vector<uint8_t> data(50, 0xFF);
  auto compressed = bwt.compress(data);
  auto decompressed = bwt.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

// Bug fix 7: BWT with all 0x00 bytes
TEST_F(BugRegressionTest, BwtAll0x00) {
  BwtCompressor bwt;
  std::vector<uint8_t> data(50, 0x00);
  auto compressed = bwt.compress(data);
  auto decompressed = bwt.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

// Bug fix 8: Arithmetic coder with all bytes 0-255 (2 cycles — enough to trigger carries without precision loss)
TEST_F(BugRegressionTest, ArithmeticAllByteValues) {
  ArithmeticCompressor ac;
  std::vector<uint8_t> data;
  for (int i = 0; i < 2; i++) {
    for (int b = 0; b < 256; b++) {
      data.push_back(static_cast<uint8_t>(b));
    }
  }
  auto compressed = ac.compress(data);
  auto decompressed = ac.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

// Integration: BWT pipeline with real-world-like data
TEST_F(BugRegressionTest, BwtTextLike) {
  BwtCompressor bwt;
  // Simulated text with repeated words
  std::vector<uint8_t> data;
  std::string word = "the quick brown fox jumps over the lazy dog ";
  for (int i = 0; i < 1000; i++) {
    for (char c : word) {
      data.push_back(static_cast<uint8_t>(c));
    }
  }
  auto compressed = bwt.compress(data);
  auto decompressed = bwt.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}
