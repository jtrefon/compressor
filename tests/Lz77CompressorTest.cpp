// tests/Lz77CompressorTest.cpp
#include <gtest/gtest.h>
#include <compression/Lz77Compressor.hpp>
#include <vector>
#include <string>
#include <cstddef> // For std::byte
#include <stdexcept>

// Helper function to convert string to vector<byte>
static std::vector<std::byte> stringToBytes(const std::string& str) {
    std::vector<std::byte> bytes(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        bytes[i] = static_cast<std::byte>(str[i]);
    }
    return bytes;
}

// Helper function to convert vector<byte> to string
static std::string bytesToString(const std::vector<std::byte>& bytes) {
    std::string str(bytes.size(), '\0');
    for (size_t i = 0; i < bytes.size(); ++i) {
        str[i] = static_cast<char>(bytes[i]);
    }
    return str;
}

// Test fixture for LZ77 tests
class Lz77CompressorTest : public ::testing::Test {
protected:
    compression::Lz77Compressor compressor; // Using default buffer sizes
    // Can create compressors with different buffer sizes if needed for specific tests
};

// --- Compression and Decompression Tests ---

TEST_F(Lz77CompressorTest, EmptyData) {
    std::vector<std::byte> data = stringToBytes("");
    std::vector<std::byte> compressed = compressor.compress(data);
    EXPECT_TRUE(compressed.empty());
    std::vector<std::byte> decompressed = compressor.decompress(compressed);
    EXPECT_TRUE(decompressed.empty());
}

TEST_F(Lz77CompressorTest, ShortDataNoMatches) {
    std::vector<std::byte> data = stringToBytes("AB"); // Shorter than MIN_MATCH_LENGTH
    std::vector<std::byte> compressed = compressor.compress(data);
    // Expected: Literal A, Literal B
    // Encoding: [Flag=0][A], [Flag=0][B]
    EXPECT_EQ(compressed.size(), 4);
    EXPECT_EQ(compressed[0], compression::Lz77Compressor::LITERAL_FLAG);
    EXPECT_EQ(compressed[1], static_cast<std::byte>('A'));
    EXPECT_EQ(compressed[2], compression::Lz77Compressor::LITERAL_FLAG);
    EXPECT_EQ(compressed[3], static_cast<std::byte>('B'));

    std::vector<std::byte> decompressed = compressor.decompress(compressed);
    EXPECT_EQ(bytesToString(decompressed), "AB");
}

TEST_F(Lz77CompressorTest, SimpleRepeatingPattern) {
    std::string original = "ABABABABABABABAB"; // 16 bytes
    std::vector<std::byte> data = stringToBytes(original);
    std::vector<std::byte> compressed = compressor.compress(data);

    // Expected: Lit(A), Lit(B), Pair(dist=2, len=14) - length might be clamped by lookahead buffer
    // Default lookahead is 32, so len=14 should be fine.
    // Encoding: [0][A], [0][B], [1][DistLow=2][DistHigh=0][Len=14]
    // Total size = 2 + 2 + 4 = 8 bytes (significant compression)

    // Verify decompression restores original data
    std::vector<std::byte> decompressed = compressor.decompress(compressed);
    EXPECT_EQ(bytesToString(decompressed), original);
    EXPECT_LT(compressed.size(), data.size()); // Expect compression
    // Add more specific checks on compressed data structure if needed
}

TEST_F(Lz77CompressorTest, LongerRepeatingPattern) {
    std::string original = "ABCABCABCABCABCABCABC"; // 21 bytes
    std::vector<std::byte> data = stringToBytes(original);
    std::vector<std::byte> compressed = compressor.compress(data);

    // Expected: Lit(A), Lit(B), Lit(C), Pair(dist=3, len=18)
    // Encoding: [0][A], [0][B], [0][C], [1][DistLow=3][DistHigh=0][Len=18]
    // Total size = 2 + 2 + 2 + 4 = 10 bytes

    std::vector<std::byte> decompressed = compressor.decompress(compressed);
    EXPECT_EQ(bytesToString(decompressed), original);
    EXPECT_LT(compressed.size(), data.size());
}


TEST_F(Lz77CompressorTest, OverlappingMatch) {
    // Creates pattern where match source overlaps destination copy range
    std::string original = "ABCABCABCABCDEFDEFDEF";
    std::vector<std::byte> data = stringToBytes(original);
    std::vector<std::byte> compressed = compressor.compress(data);

    // Example trace:
    // Lit A, Lit B, Lit C
    // Match "ABC" at current pos 3, looking back finds "ABC" at pos 0. Output Pair(dist=3, len=3) -> pos=6
    // Match "ABC" at current pos 6, looking back finds "ABC" at pos 3. Output Pair(dist=3, len=3) -> pos=9
    // Match "ABC" at current pos 9, looking back finds "ABC" at pos 6. Output Pair(dist=3, len=3) -> pos=12
    // Lit D, Lit E, Lit F
    // Match "DEF" at current pos 15, looking back finds "DEF" at pos 12. Output Pair(dist=3, len=3) -> pos=18
    // Match "DEF" at current pos 18, looking back finds "DEF" at pos 15. Output Pair(dist=3, len=3) -> pos=21

    std::vector<std::byte> decompressed = compressor.decompress(compressed);
    EXPECT_EQ(bytesToString(decompressed), original);
    EXPECT_LT(compressed.size(), data.size());
}


TEST_F(Lz77CompressorTest, MixedLiteralsAndMatches) {
    std::string original = "This is a test string with some repeating test string parts.";
    std::vector<std::byte> data = stringToBytes(original);
    std::vector<std::byte> compressed = compressor.compress(data);
    std::vector<std::byte> decompressed = compressor.decompress(compressed);
    EXPECT_EQ(bytesToString(decompressed), original);
    // EXPECT_LT(compressed.size(), data.size()); // REMOVED: Basic LZ77 can expand data like this.
}

TEST_F(Lz77CompressorTest, DataRequiresMaxDistance) {
    // Create data where match is far back (close to search buffer size)
    std::string prefix(4000, 'A'); // Fill search buffer mostly
    std::string match = "XYZ";
    std::string suffix(50, 'B');
    std::string original = match + prefix + suffix + match; // Match is > 4000 bytes away
    std::vector<std::byte> data = stringToBytes(original);

    compression::Lz77Compressor compressor_large_win(4096, 32); // Ensure window is large enough
    std::vector<std::byte> compressed = compressor_large_win.compress(data);
    std::vector<std::byte> decompressed = compressor_large_win.decompress(compressed);

    EXPECT_EQ(bytesToString(decompressed), original);
    // Check if the last match was indeed encoded as a pair (indicating distance was reachable)
}


TEST_F(Lz77CompressorTest, DataRequiresMaxLength) {
    // Create data with a run longer than max encodable length (default 32)
    std::string original = "START_" + std::string(50, 'X') + "_END";
    std::vector<std::byte> data = stringToBytes(original);
    std::vector<std::byte> compressed = compressor.compress(data);

    // Expected: Literals S,T,A,R,T,_ ; Pair(dist=1, len=MAX_LENGTH=32); Pair(dist=1, len=12); Literals _,E,N,D
    // Verify correct decompression
    std::vector<std::byte> decompressed = compressor.decompress(compressed);
    EXPECT_EQ(bytesToString(decompressed), original);
}


// --- Decompression Error Tests ---

TEST_F(Lz77CompressorTest, DecompressEmpty) {
    // This is valid, should produce empty output
    std::vector<std::byte> compressed = {};
    std::vector<std::byte> decompressed = compressor.decompress(compressed);
    EXPECT_TRUE(decompressed.empty());
}

TEST_F(Lz77CompressorTest, DecompressTruncatedLiteralFlagOnly) {
    std::vector<std::byte> compressed = {compression::Lz77Compressor::LITERAL_FLAG};
    EXPECT_THROW(compressor.decompress(compressed), std::runtime_error);
}

TEST_F(Lz77CompressorTest, DecompressTruncatedPairFlagOnly) {
    std::vector<std::byte> compressed = {compression::Lz77Compressor::PAIR_FLAG};
    EXPECT_THROW(compressor.decompress(compressed), std::runtime_error);
}

TEST_F(Lz77CompressorTest, DecompressTruncatedPairMissingLength) {
    std::vector<std::byte> compressed = {
        compression::Lz77Compressor::PAIR_FLAG,
        std::byte{2}, std::byte{0} // Distance = 2
        // Missing Length byte
    };
    EXPECT_THROW(compressor.decompress(compressed), std::runtime_error);
}

TEST_F(Lz77CompressorTest, DecompressTruncatedPairMissingDistHigh) {
    std::vector<std::byte> compressed = {
        compression::Lz77Compressor::PAIR_FLAG,
        std::byte{2} // Distance low byte
        // Missing Distance high byte
        // Missing Length byte
    };
    EXPECT_THROW(compressor.decompress(compressed), std::runtime_error);
}


TEST_F(Lz77CompressorTest, DecompressInvalidFlag) {
    std::vector<std::byte> compressed = {
        compression::Lz77Compressor::LITERAL_FLAG, std::byte{'A'},
        std::byte{42}, // Invalid flag
        compression::Lz77Compressor::LITERAL_FLAG, std::byte{'B'}
    };
    EXPECT_THROW(compressor.decompress(compressed), std::runtime_error);
}

TEST_F(Lz77CompressorTest, DecompressInvalidDistanceZero) {
     // Compressed: Lit(A), Lit(B), Lit(C), Pair(dist=0, len=3) <- Invalid distance
     std::vector<std::byte> compressed = {
        compression::Lz77Compressor::LITERAL_FLAG, std::byte{'A'},
        compression::Lz77Compressor::LITERAL_FLAG, std::byte{'B'},
        compression::Lz77Compressor::LITERAL_FLAG, std::byte{'C'},
        compression::Lz77Compressor::PAIR_FLAG,
        std::byte{0}, std::byte{0}, // Distance = 0 (Invalid)
        std::byte{3} // Length = 3
    };
    EXPECT_THROW(compressor.decompress(compressed), std::runtime_error);
}

TEST_F(Lz77CompressorTest, DecompressInvalidDistanceTooLarge) {
    // Compressed: Lit(A), Lit(B), Lit(C), Pair(dist=5, len=3) <- Distance > decompressed size (3)
     std::vector<std::byte> compressed = {
        compression::Lz77Compressor::LITERAL_FLAG, std::byte{'A'},
        compression::Lz77Compressor::LITERAL_FLAG, std::byte{'B'},
        compression::Lz77Compressor::LITERAL_FLAG, std::byte{'C'},
        compression::Lz77Compressor::PAIR_FLAG,
        std::byte{5}, std::byte{0}, // Distance = 5 (Invalid, only 3 bytes available)
        std::byte{3} // Length = 3
    };
    EXPECT_THROW(compressor.decompress(compressed), std::runtime_error);
}

TEST_F(Lz77CompressorTest, DecompressInvalidLengthTooSmall) {
    // Note: The compressor shouldn't produce lengths < MIN_MATCH_LENGTH for pairs,
    // but we test if the decompressor handles receiving such (malformed) data.
     std::vector<std::byte> compressed = {
        compression::Lz77Compressor::LITERAL_FLAG, std::byte{'A'},
        compression::Lz77Compressor::LITERAL_FLAG, std::byte{'B'},
        compression::Lz77Compressor::LITERAL_FLAG, std::byte{'C'},
        compression::Lz77Compressor::PAIR_FLAG,
        std::byte{1}, std::byte{0}, // Distance = 1
        std::byte{2} // Length = 2 (Invalid, < MIN_MATCH_LENGTH=3)
    };
    // Depending on implementation detail, this might throw or just behave unexpectedly.
    // Let's expect a throw based on the check in decompress.
    EXPECT_THROW(compressor.decompress(compressed), std::runtime_error);
} 