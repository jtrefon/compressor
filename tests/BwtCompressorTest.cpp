#include <gtest/gtest.h>
#include <compression/BwtCompressor.hpp>
#include <vector>
#include <string>
#include <cstdint>

// Helper function to convert string to vector<uint8_t>
static std::vector<uint8_t> stringToBytes(const std::string& str) {
    return std::vector<uint8_t>(str.begin(), str.end());
}

// Helper function to convert vector<uint8_t> to string
static std::string bytesToString(const std::vector<uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

// Test fixture for BWT tests
class BwtCompressorTest : public ::testing::Test {
protected:
    compression::BwtCompressor compressor;
};

TEST_F(BwtCompressorTest, EmptyData) {
    auto data = stringToBytes("");
    auto compressed = compressor.compress(data);
    EXPECT_TRUE(compressed.empty());
    auto decompressed = compressor.decompress(compressed);
    EXPECT_EQ(bytesToString(decompressed), "");
}

TEST_F(BwtCompressorTest, SimpleString) {
    std::string original = "abcdefg";
    auto data = stringToBytes(original);
    auto compressed = compressor.compress(data);
    auto decompressed = compressor.decompress(compressed);
    EXPECT_EQ(bytesToString(decompressed), original);
}

TEST_F(BwtCompressorTest, BananaTest) {
    std::string original = "banana";
    auto data = stringToBytes(original);

    // Expected BWT of "banana^" (with a sentinel) is "annb^aa" with index 3.
    // Without a sentinel, the rotations are sorted differently.
    // Let's just verify correctness via round-trip.
    auto compressed = compressor.compress(data);
    auto decompressed = compressor.decompress(compressed);
    
    EXPECT_EQ(bytesToString(decompressed), original);
}

TEST_F(BwtCompressorTest, StringWithRepeats) {
    std::string original = "abracaalabama";
    auto data = stringToBytes(original);
    auto compressed = compressor.compress(data);
    auto decompressed = compressor.decompress(compressed);
    EXPECT_EQ(bytesToString(decompressed), original);
}

TEST_F(BwtCompressorTest, AllSameCharacters) {
    std::string original = "AAAAAAAAAA";
    auto data = stringToBytes(original);
    auto compressed = compressor.compress(data);
    auto decompressed = compressor.decompress(compressed);
    EXPECT_EQ(bytesToString(decompressed), original);
}

TEST_F(BwtCompressorTest, DecompressInvalidDataTooShort) {
    std::vector<uint8_t> invalid_data = {1, 2, 3}; // Shorter than the 4-byte index
    EXPECT_THROW(compressor.decompress(invalid_data), std::runtime_error);
}

TEST_F(BwtCompressorTest, DecompressInvalidDataBadIndex) {
    std::string original = "test";
    auto data = stringToBytes(original);
    auto compressed = compressor.compress(data);
    
    // Manually corrupt the primary index to be out of bounds
    compressed[0] = 0xFF; // Set a large index
    compressed[1] = 0xFF;
    
    EXPECT_THROW(compressor.decompress(compressed), std::runtime_error);
} 