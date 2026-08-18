#include <algorithm>
#include <compression/BwtCompressor.hpp>
#include <compression/NullCompressor.hpp>
#include <cstdint>
#include <gtest/gtest.h>
#include <iterator>
#include <string>
#include <vector>

// Helper function to create vector<uint8_t> from string
std::vector<uint8_t> stringToBytes(const std::string &str) {
  std::vector<uint8_t> bytes;
  bytes.reserve(str.size());
  std::transform(str.begin(), str.end(), std::back_inserter(bytes),
                 [](char c) { return static_cast<uint8_t>(c); });
  return bytes;
}

// Helper function to create vector<uint8_t> from initializer list of ints
std::vector<uint8_t> bytesFromInts(std::initializer_list<int> ints) {
  std::vector<uint8_t> bytes;
  bytes.reserve(ints.size());
  std::transform(ints.begin(), ints.end(), std::back_inserter(bytes),
                 [](int i) { return static_cast<uint8_t>(i); });
  return bytes;
}

// Demonstrate some basic assertions.
TEST(HelloTest, BasicAssertions) {
  // Expect two strings not to be equal.
  EXPECT_STRNE("hello", "world");
  // Expect equality.
  EXPECT_EQ(7 * 6, 42);
}

// Test fixture for NullCompressor tests
class NullCompressorTest : public ::testing::Test {
protected:
  compression::NullCompressor compressor;
  std::vector<uint8_t> testData;

  void SetUp() override {
    // Initialize test data
    std::string s = "Hello, world!";
    testData.resize(s.size());
    std::transform(s.begin(), s.end(), testData.begin(),
                   [](char c) { return static_cast<uint8_t>(c); });
  }
};

TEST_F(NullCompressorTest, CompressReturnsOriginalData) {
  std::vector<uint8_t> compressedData = compressor.compress(testData);
  ASSERT_EQ(compressedData.size(), testData.size());
  EXPECT_EQ(compressedData, testData);
}

TEST_F(NullCompressorTest, DecompressReturnsOriginalData) {
  // Since it's a null compressor, the 'compressed' data is the original
  std::vector<uint8_t> decompressedData = compressor.decompress(testData);
  ASSERT_EQ(decompressedData.size(), testData.size());
  EXPECT_EQ(decompressedData, testData);
}

TEST_F(NullCompressorTest, EmptyData) {
  std::vector<uint8_t> emptyData;
  std::vector<uint8_t> compressedData = compressor.compress(emptyData);
  EXPECT_TRUE(compressedData.empty());
  std::vector<uint8_t> decompressedData = compressor.decompress(emptyData);
  EXPECT_TRUE(decompressedData.empty());
}

// Main function for running tests
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
