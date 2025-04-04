#include <gtest/gtest.h>
#include "compression/ArithmeticCompressor.hpp"
#include "compression/HuffmanCompressor.hpp"
#include <vector>
#include <string>
#include <random>
#include <algorithm>

namespace compression {
namespace test {

class ArithmeticCompressorTest : public ::testing::Test {
protected:
    ArithmeticCompressor compressor;
};

// Test with empty input
TEST_F(ArithmeticCompressorTest, EmptyInput) {
    std::vector<uint8_t> emptyData;
    std::vector<uint8_t> compressed = compressor.compress(emptyData);
    std::vector<uint8_t> decompressed = compressor.decompress(compressed);
    
    EXPECT_TRUE(decompressed.empty());
}

// Test with a simple string
TEST_F(ArithmeticCompressorTest, SimpleString) {
    std::string input = "HelloWorld";
    std::vector<uint8_t> data(input.begin(), input.end());
    
    std::vector<uint8_t> compressed = compressor.compress(data);
    std::vector<uint8_t> decompressed = compressor.decompress(compressed);
    
    EXPECT_EQ(data.size(), decompressed.size());
    EXPECT_EQ(data, decompressed);
}

// Test with repeated characters
TEST_F(ArithmeticCompressorTest, RepeatedCharacters) {
    std::vector<uint8_t> data(1000, 'A');
    
    std::vector<uint8_t> compressed = compressor.compress(data);
    std::vector<uint8_t> decompressed = compressor.decompress(compressed);
    
    // Check compression ratio
    double ratio = static_cast<double>(compressed.size()) / static_cast<double>(data.size());
    std::cout << "Compression ratio for repeated characters: " << ratio << std::endl;
    
    EXPECT_EQ(data.size(), decompressed.size());
    EXPECT_EQ(data, decompressed);
    
    // Expect good compression for repeated characters
    EXPECT_LT(ratio, 0.1); // Should compress to less than 10% of original size
}

// Test with random data
TEST_F(ArithmeticCompressorTest, RandomData) {
    std::vector<uint8_t> data(1000);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 255);
    
    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dist(gen));
    }
    
    std::vector<uint8_t> compressed = compressor.compress(data);
    std::vector<uint8_t> decompressed = compressor.decompress(compressed);
    
    EXPECT_EQ(data.size(), decompressed.size());
    EXPECT_EQ(data, decompressed);
}

// Test with all possible byte values
TEST_F(ArithmeticCompressorTest, AllByteValues) {
    // Create a vector with all possible byte values (0-255)
    std::vector<uint8_t> data(256);
    for (int i = 0; i < 256; ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    
    // Add repetitions to make the data more compressible
    for (int i = 0; i < 3; ++i) {
        data.insert(data.end(), data.begin(), data.begin() + 256);
    }
    
    // Use a fixed seed for consistent results
    unsigned seed = 42;
    std::shuffle(data.begin(), data.end(), std::default_random_engine(seed));
    
    std::vector<uint8_t> compressed = compressor.compress(data);
    std::vector<uint8_t> decompressed = compressor.decompress(compressed);
    
    // Verify compression results
    EXPECT_EQ(data.size(), decompressed.size());
    EXPECT_EQ(data, decompressed);
    
    // Compare compression ratio with Huffman
    HuffmanCompressor huffmanCompressor;
    std::vector<uint8_t> huffmanCompressed = huffmanCompressor.compress(data);
    
    double arithmeticRatio = static_cast<double>(compressed.size()) / static_cast<double>(data.size());
    double huffmanRatio = static_cast<double>(huffmanCompressed.size()) / static_cast<double>(data.size());
    
    std::cout << "Arithmetic compression ratio: " << arithmeticRatio << std::endl;
    std::cout << "Huffman compression ratio: " << huffmanRatio << std::endl;
    
    // Arithmetic should be better than Huffman
    EXPECT_LT(arithmeticRatio, huffmanRatio);
}

// Test with a larger text file
TEST_F(ArithmeticCompressorTest, LargeText) {
    // Generate a larger text file with realistic distribution
    std::vector<uint8_t> data;
    data.reserve(10000);
    
    // Add some common English text patterns
    std::string text = "The quick brown fox jumps over the lazy dog. ";
    text += "Lorem ipsum dolor sit amet, consectetur adipiscing elit. ";
    text += "This is a test of the arithmetic coding compression algorithm. ";
    text += "It should provide better compression ratios than Huffman coding for many types of data. ";
    
    // Repeat the text to make a larger file
    for (int i = 0; i < 50; ++i) {
        data.insert(data.end(), text.begin(), text.end());
    }
    
    std::vector<uint8_t> compressed = compressor.compress(data);
    std::vector<uint8_t> decompressed = compressor.decompress(compressed);
    
    // Compare with Huffman compression
    HuffmanCompressor huffmanCompressor;
    std::vector<uint8_t> huffmanCompressed = huffmanCompressor.compress(data);
    
    double arithmeticRatio = static_cast<double>(compressed.size()) / static_cast<double>(data.size());
    double huffmanRatio = static_cast<double>(huffmanCompressed.size()) / static_cast<double>(data.size());
    
    std::cout << "Large text arithmetic compression ratio: " << arithmeticRatio << std::endl;
    std::cout << "Large text Huffman compression ratio: " << huffmanRatio << std::endl;
    
    EXPECT_EQ(data.size(), decompressed.size());
    EXPECT_EQ(data, decompressed);
    
    // Arithmetic should provide better compression than Huffman
    EXPECT_LT(arithmeticRatio, huffmanRatio);
}

// Test error handling
TEST_F(ArithmeticCompressorTest, InvalidInput) {
    // Create an invalid compressed data buffer
    std::vector<uint8_t> invalidData = {'C', 'P', 'R', 'O', 1, 4}; // Invalid header
    
    // Decompression should throw an exception
    EXPECT_THROW(compressor.decompress(invalidData), std::runtime_error);
}

} // namespace test
} // namespace compression 