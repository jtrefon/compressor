#include <gtest/gtest.h>
#include <compression/dummy.hpp>
#include <compression/NullCompressor.hpp>
#include <compression/RleCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <vector>
#include <cstdint>
#include <string>
#include <algorithm>
#include <iterator>
#include <random> // For std::shuffle, std::mt19937
#include <chrono> // For seeding random generator

// Helper function to create vector<uint8_t> from string
std::vector<uint8_t> stringToBytes(const std::string& str) {
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

TEST(CompressionLibTest, DummyFunctionTest) {
    // This test doesn't assert anything about the output,
    // but confirms the function can be called.
    ASSERT_NO_THROW(compression::dummy_function());
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
        std::transform(s.begin(), s.end(), testData.begin(), [](char c) {
            return static_cast<uint8_t>(c);
        });
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

// --- RleCompressor Tests --- 

class RleCompressorTest : public ::testing::Test {
protected:
    compression::RleCompressor compressor;
};

TEST_F(RleCompressorTest, EmptyData) {
    std::vector<uint8_t> emptyData;
    auto compressed = compressor.compress(emptyData);
    EXPECT_TRUE(compressed.empty());
    auto decompressed = compressor.decompress(compressed);
    EXPECT_TRUE(decompressed.empty());
}

TEST_F(RleCompressorTest, SingleByte) {
    auto data = bytesFromInts({65}); // 'A'
    auto compressed = compressor.compress(data);
    // Expected: count=1, value=65
    EXPECT_EQ(compressed, bytesFromInts({1, 65}));
    auto decompressed = compressor.decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST_F(RleCompressorTest, SimpleRun) {
    auto data = stringToBytes("AAAAABBB");
    auto compressed = compressor.compress(data);
    // Expected: count=5, value='A', count=3, value='B'
    EXPECT_EQ(compressed, bytesFromInts({5, 'A', 3, 'B'}));
    auto decompressed = compressor.decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST_F(RleCompressorTest, NoRuns) {
    auto data = stringToBytes("ABCDEFG");
    auto compressed = compressor.compress(data);
    // Expected: 1A 1B 1C 1D 1E 1F 1G
    EXPECT_EQ(compressed, bytesFromInts({1,'A', 1,'B', 1,'C', 1,'D', 1,'E', 1,'F', 1,'G'}));
    EXPECT_GT(compressed.size(), data.size()); // RLE made it larger
    auto decompressed = compressor.decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST_F(RleCompressorTest, MaxRunLength) {
    std::string longRunStr(255, 'X');
    auto data = stringToBytes(longRunStr + "Y");
    auto compressed = compressor.compress(data);
    // Expected: count=255, value='X', count=1, value='Y'
    EXPECT_EQ(compressed, bytesFromInts({255, 'X', 1, 'Y'}));
    auto decompressed = compressor.decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST_F(RleCompressorTest, MultipleMaxRuns) {
    std::string longRunStr(515, 'Z'); // 255 + 255 + 5
    auto data = stringToBytes(longRunStr);
    auto compressed = compressor.compress(data);
    // Expected: 255 Z, 255 Z, 5 Z
    EXPECT_EQ(compressed, bytesFromInts({255, 'Z', 255, 'Z', 5, 'Z'}));
    auto decompressed = compressor.decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST_F(RleCompressorTest, MixedRuns) {
    auto data = stringToBytes("AAABBCDDDDEFF");
    auto compressed = compressor.compress(data);
    // Expected: 3A 2B 1C 4D 1E 2F
    EXPECT_EQ(compressed, bytesFromInts({3,'A', 2,'B', 1,'C', 4,'D', 1,'E', 2,'F'}));
    auto decompressed = compressor.decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST_F(RleCompressorTest, DecompressInvalidSize) {
    auto invalidData = bytesFromInts({3, 'A', 2}); // Odd number of bytes
    EXPECT_THROW(compressor.decompress(invalidData), std::runtime_error);
}

TEST_F(RleCompressorTest, DecompressZeroCount) {
    auto invalidData = bytesFromInts({0, 'A', 2, 'B'}); // Zero count
    EXPECT_THROW(compressor.decompress(invalidData), std::runtime_error);
}

// --- HuffmanCompressor Tests --- 

class HuffmanCompressorTest : public ::testing::Test {
protected:
    compression::HuffmanCompressor compressor;

    // Helper to check round trip
    void testRoundTrip(const std::vector<uint8_t>& data) {
        SCOPED_TRACE("Testing round trip with data size: " + std::to_string(data.size()));
        std::vector<uint8_t> compressed;
        ASSERT_NO_THROW(compressed = compressor.compress(data));
        
        // Basic check: compression shouldn't throw for valid input
        // We don't assert size reduction as Huffman can increase size for incompressible data

        std::vector<uint8_t> decompressed;
        ASSERT_NO_THROW(decompressed = compressor.decompress(compressed));
        
        EXPECT_EQ(decompressed.size(), data.size());
        EXPECT_EQ(decompressed, data);
    }
};

TEST_F(HuffmanCompressorTest, EmptyData) {
    std::vector<uint8_t> emptyData;
    testRoundTrip(emptyData);
    // Also test direct compress/decompress
    auto compressed = compressor.compress(emptyData);
    EXPECT_TRUE(compressed.empty());
    auto decompressed = compressor.decompress(compressed);
    EXPECT_TRUE(decompressed.empty());
}

TEST_F(HuffmanCompressorTest, SingleByte) {
    testRoundTrip(bytesFromInts({65})); // 'A'
}

TEST_F(HuffmanCompressorTest, RepeatedByte) {
    testRoundTrip(stringToBytes("AAAAAAAAAA")); // AAAAA... (10 times)
}

TEST_F(HuffmanCompressorTest, TwoBytes) {
    testRoundTrip(stringToBytes("ABABABABAB")); // ABABAB... (10 times)
}

TEST_F(HuffmanCompressorTest, SimpleString) {
    testRoundTrip(stringToBytes("hello world"));
}

TEST_F(HuffmanCompressorTest, LongerStringWithVaryingFreq) {
    testRoundTrip(stringToBytes("this is a test string with several repeated characters"));
}

TEST_F(HuffmanCompressorTest, AllByteValues) {
    std::vector<uint8_t> allBytes;
    allBytes.reserve(256);
    for (int i = 0; i < 256; ++i) {
        allBytes.push_back(static_cast<uint8_t>(i));
    }
    // Add some repetitions to make it compressible
    for (int i = 0; i < 128; ++i) {
         allBytes.push_back(static_cast<uint8_t>(i));
    }
    // Use std::shuffle instead of std::random_shuffle
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::shuffle(allBytes.begin(), allBytes.end(), std::default_random_engine(seed)); 
    testRoundTrip(allBytes);
}

TEST_F(HuffmanCompressorTest, IncompressibleData) {
    // Data that likely won't compress well (or might expand) with Huffman
    testRoundTrip(stringToBytes("abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
}

TEST_F(HuffmanCompressorTest, DecompressEmptyData) {
    std::vector<uint8_t> emptyData;
    std::vector<uint8_t> decompressed;
    // Decompressing truly empty data should result in empty data
    ASSERT_NO_THROW(decompressed = compressor.decompress(emptyData));
    EXPECT_TRUE(decompressed.empty());
}

TEST_F(HuffmanCompressorTest, DecompressInvalidData_TooShortForHeader) {
     // Needs freq map size (2) + bit count (8) = 10 bytes minimum conceptually
    auto shortData = bytesFromInts({1, 2, 3}); 
    EXPECT_THROW(compressor.decompress(shortData), std::runtime_error);
}

TEST_F(HuffmanCompressorTest, DecompressInvalidData_TruncatedPayload) {
    // Compress valid data first
    auto originalData = stringToBytes("some data");
    std::vector<uint8_t> compressed;
    ASSERT_NO_THROW(compressed = compressor.compress(originalData));
    ASSERT_GT(compressed.size(), 10); // Ensure it has some payload

    // Truncate the compressed data (remove last byte)
    std::vector<uint8_t> truncatedData(compressed.begin(), compressed.end() - 1);
    
    // Decompression should ideally throw due to missing bits or invalid final state
    EXPECT_THROW(compressor.decompress(truncatedData), std::runtime_error); 
}

// GoogleTest main function is usually sufficient
// int main(int argc, char **argv) {
//   ::testing::InitGoogleTest(&argc, argv);
//   return RUN_ALL_TESTS();
// } 