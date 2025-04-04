#ifndef COMPRESSION_DEFLATECOMPRESSOR_HPP
#define COMPRESSION_DEFLATECOMPRESSOR_HPP

#include "ICompressor.hpp"
#include "Lz77Compressor.hpp" // To get symbols
#include "HuffmanCoder.hpp"
#include "BitIO.hpp"

#include <vector>
#include <map>
#include <cstdint>
#include <memory>
#include <stdexcept> // Include for potential exceptions
#include <iostream> // For verbose logging
#include <algorithm> // For std::min, std::max, std::sort
#include <cmath> // For log2 (optional)
#include <limits> // For numeric_limits
#include <cstddef> // For byte
#include <string>

namespace compression {

/**
 * @brief Helper structure for RLE of code lengths
 */
struct RleSymbol {
    uint8_t symbol; // 0-15 (length), 16, 17, 18 (repeat codes)
    uint8_t extraBitsValue = 0; // Value for the extra bits (if symbol 16, 17, 18)
    uint8_t extraBitsCount = 0; // Number of extra bits (0, 2, 3, or 7)
};

// Public Type Aliases (used by HuffmanCoder)
using HuffmanCode = std::vector<bool>; // Represents a single Huffman code
using HuffmanCodeMap = std::map<uint32_t, HuffmanCode>; // Map symbol to code
using FrequencyMap = std::map<uint32_t, uint64_t>;

/**
 * @brief Huffman Decoding Tree Node
 */
struct HuffmanDecoderNode {
    uint32_t symbol = 0; // Valid only if isLeaf is true
    bool isLeaf = false;
    std::unique_ptr<HuffmanDecoderNode> left = nullptr;
    std::unique_ptr<HuffmanDecoderNode> right = nullptr;

    HuffmanDecoderNode() = default; 
};

// Function Declaration for Building Decoding Tree
std::unique_ptr<HuffmanDecoderNode> buildDecodingTree(const HuffmanCodeMap& codeMap);

/**
 * @brief Adapter class to convert between uint8_t and byte types
 * 
 * This follows the Adapter pattern to handle the type conversion necessary
 * between the interface and implementation.
 */
class ByteTypeAdapter {
public:
    // Convert byte type to uint8_t
    static std::vector<uint8_t> byteToUint8(const std::vector<unsigned char>& bytes) {
        std::vector<uint8_t> result(bytes.size());
        for (size_t i = 0; i < bytes.size(); i++) {
            result[i] = static_cast<uint8_t>(bytes[i]);
        }
        return result;
    }
    
    // Convert uint8_t to byte type
    static std::vector<unsigned char> uint8ToByte(const std::vector<uint8_t>& data) {
        std::vector<unsigned char> result(data.size());
        for (size_t i = 0; i < data.size(); i++) {
            result[i] = static_cast<unsigned char>(data[i]);
        }
        return result;
    }
};

/**
 * @brief Implements the Deflate compression algorithm.
 * 
 * Deflate combines LZ77 compression with Huffman coding.
 * This implementation currently uses an optimized LZ77 implementation
 * and will be enhanced with Huffman coding in future versions.
 */
class DeflateCompressor : public ICompressor {
public:
    /**
     * @brief Construct a DeflateCompressor with default settings.
     * 
     * Uses an optimized LZ77 compressor internally.
     */
    DeflateCompressor();
    
    /**
     * @brief Destructor with default implementation.
     */
    ~DeflateCompressor() override;
    
    /**
     * @brief Compresses data using the Deflate algorithm.
     * 
     * Currently uses an optimized LZ77 compression as the first step.
     * 
     * @param data The data to compress.
     * @return The compressed data.
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
    
    /**
     * @brief Decompresses data that was compressed with the Deflate algorithm.
     * 
     * @param data The compressed data.
     * @return The decompressed data.
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;

private:
    // --- Internal Helpers --- 
    
    // Function to build frequency maps for literal/length and distance symbols
    void buildFrequencyMaps(
        const std::vector<Lz77Compressor::Lz77Symbol>& symbols,
        FrequencyMap& litLenFreqMap,
        FrequencyMap& distFreqMap) const;

    // Function to encode the LZ77 symbols using the Huffman codes
    void encodeSymbols(
        BitIO::BitWriter& bitWriter,
        const std::vector<Lz77Compressor::Lz77Symbol>& symbols,
        const HuffmanCodeMap& litLenCodeMap,
        const HuffmanCodeMap& distCodeMap) const;

    // Helper for writing dynamic tables according to Deflate spec
    void writeDynamicTables(
        BitIO::BitWriter& writer, 
        const HuffmanCodeMap& litLenCodes, 
        const HuffmanCodeMap& distCodes) const;

    // Helper for reading dynamic tables according to Deflate spec
    std::pair<HuffmanCodeMap, HuffmanCodeMap> readDynamicTables(BitIO::BitReader& reader) const;

    void decodeSymbols(
        BitIO::BitReader& reader,
        const HuffmanDecoderNode& litLenTreeRoot,
        const HuffmanDecoderNode& distTreeRoot, 
        std::vector<uint8_t>& output) const;

    uint32_t decodeSymbol(BitIO::BitReader& bitReader, const HuffmanDecoderNode* root) const;

    // RLE Encoding for Code Lengths
    std::vector<RleSymbol> runLengthEncodeCodeLengths(const std::vector<uint8_t>& lengths) const;

    // --- Member Variables --- 
    std::unique_ptr<Lz77Compressor> lz77_; // LZ77 compressor
    HuffmanCoder huffmanCoder_; // Extracted Huffman coding logic
    bool verbose_ = false; // Verbosity flag for debugging
};

} // namespace compression

#endif // COMPRESSION_DEFLATECOMPRESSOR_HPP 