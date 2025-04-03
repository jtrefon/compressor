#pragma once

#include <cstddef> // For std::byte
#include <cstdint> // For uint8_t, uint64_t
#include <array>
#include <vector>
#include <string>
#include <stdexcept> // For std::runtime_error
#include <algorithm> // For std::copy

namespace compression {
namespace format {

// --- Constants --- 

constexpr std::array<std::byte, 4> MAGIC_NUMBER = {
    std::byte{'C'}, std::byte{'P'}, std::byte{'R'}, std::byte{'O'}
};
constexpr uint8_t FORMAT_VERSION = 1;

// Algorithm IDs (extend this as new algorithms are added)
enum class AlgorithmID : uint8_t {
    NULL_COMPRESSOR = 0,
    RLE_COMPRESSOR = 1,
    HUFFMAN_COMPRESSOR = 2,
    // Add future IDs here
    UNKNOWN = 255
};

constexpr size_t HEADER_SIZE = MAGIC_NUMBER.size() 
                               + sizeof(FORMAT_VERSION) 
                               + sizeof(AlgorithmID) 
                               + sizeof(uint64_t) // Original Size
                               + sizeof(uint32_t); // Original Checksum (CRC32)

// --- Header Structure (Conceptual) --- 

// We won't use a packed struct directly to avoid portability issues (padding, endianness).
// Instead, we'll use serialization/deserialization functions.
struct FileHeader {
    // Magic number is implicitly checked/written
    uint8_t formatVersion = FORMAT_VERSION;
    AlgorithmID algorithmId = AlgorithmID::UNKNOWN;
    uint64_t originalSize = 0;
    uint32_t originalChecksum = 0; // Added CRC32 checksum
};

// --- Serialization / Deserialization --- 

/**
 * @brief Serializes the header data into a byte vector.
 * @param header The header data to serialize.
 * @return A vector of bytes representing the serialized header.
 */
inline std::vector<std::byte> serializeHeader(const FileHeader& header) {
    std::vector<std::byte> buffer(HEADER_SIZE);
    size_t offset = 0;

    // 1. Magic Number
    std::copy(MAGIC_NUMBER.begin(), MAGIC_NUMBER.end(), buffer.begin() + offset);
    offset += MAGIC_NUMBER.size();

    // 2. Format Version
    buffer[offset++] = static_cast<std::byte>(header.formatVersion);

    // 3. Algorithm ID
    buffer[offset++] = static_cast<std::byte>(header.algorithmId);

    // 4. Original Size (little-endian)
    for (int i = 0; i < 8; ++i) {
        buffer[offset++] = static_cast<std::byte>((header.originalSize >> (i * 8)) & 0xFF);
    }

    // 5. Original Checksum (little-endian)
    for (int i = 0; i < 4; ++i) {
        buffer[offset++] = static_cast<std::byte>((header.originalChecksum >> (i * 8)) & 0xFF);
    }

    return buffer;
}

/**
 * @brief Deserializes header data from a byte vector.
 * @param buffer The byte vector containing the serialized header (must be at least HEADER_SIZE bytes).
 * @return The deserialized FileHeader.
 * @throws std::runtime_error if magic number or version is incorrect, or buffer is too small.
 */
inline FileHeader deserializeHeader(const std::vector<std::byte>& buffer) {
    if (buffer.size() < HEADER_SIZE) {
        throw std::runtime_error("Buffer too small to contain file header.");
    }

    size_t offset = 0;
    FileHeader header;

    // 1. Verify Magic Number
    if (!std::equal(MAGIC_NUMBER.begin(), MAGIC_NUMBER.end(), buffer.begin() + offset)) {
        throw std::runtime_error("Invalid magic number. Not a recognized compressed file.");
    }
    offset += MAGIC_NUMBER.size();

    // 2. Read and Verify Format Version
    header.formatVersion = static_cast<uint8_t>(buffer[offset++]);
    if (header.formatVersion != FORMAT_VERSION) {
        throw std::runtime_error("Unsupported format version: " + std::to_string(header.formatVersion));
    }

    // 3. Read Algorithm ID
    header.algorithmId = static_cast<AlgorithmID>(buffer[offset++]);

    // 4. Read Original Size (assuming little-endian storage)
    header.originalSize = 0;
    for (int i = 0; i < 8; ++i) {
        header.originalSize |= (static_cast<uint64_t>(buffer[offset++]) << (i * 8));
    }

    // 5. Read Original Checksum (assuming little-endian storage)
    header.originalChecksum = 0;
    for (int i = 0; i < 4; ++i) {
        header.originalChecksum |= (static_cast<uint32_t>(buffer[offset++]) << (i * 8));
    }

    return header;
}

/**
 * @brief Maps AlgorithmID enum to a string representation.
 * @param id The AlgorithmID.
 * @return String name of the algorithm (e.g., "rle", "null").
 */
inline std::string algorithmIdToString(AlgorithmID id) {
    switch (id) {
        case AlgorithmID::NULL_COMPRESSOR: return "null";
        case AlgorithmID::RLE_COMPRESSOR:  return "rle";
        case AlgorithmID::HUFFMAN_COMPRESSOR: return "huffman";
        default:                          return "unknown";
    }
}

/**
 * @brief Maps a string name to an AlgorithmID.
 * @param name The string name (e.g., "rle", "null").
 * @return The corresponding AlgorithmID, or UNKNOWN if not found.
 */
inline AlgorithmID stringToAlgorithmId(const std::string& name) {
    if (name == "null") return AlgorithmID::NULL_COMPRESSOR;
    if (name == "rle")  return AlgorithmID::RLE_COMPRESSOR;
    if (name == "huffman") return AlgorithmID::HUFFMAN_COMPRESSOR;
    // Add mappings for future algorithms
    return AlgorithmID::UNKNOWN;
}


} // namespace format
} // namespace compression 