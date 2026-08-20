#pragma once

#include <algorithm> // For std::copy
#include <array>
#include <cstdint>   // For uint8_t, uint64_t
#include <stdexcept> // For std::runtime_error
#include <string>
#include <vector>

namespace compression {
namespace format {

// --- Constants ---

constexpr std::array<uint8_t, 4> MAGIC_NUMBER = {'C', 'P', 'R', 'O'};
constexpr uint8_t FORMAT_VERSION = 1;

// --- BWT specific flags ---
// Bit 0 set if the data block was entropy encoded after BWT (MTF/RLE/Huffman)
constexpr uint8_t BWT_FLAG_TRANSFORMED = 0x01;
// Bit 1 set if the block was additionally LZ77 compressed
constexpr uint8_t BWT_FLAG_LZ77 = 0x02;

// Algorithm IDs (extend this as new algorithms are added)
enum class AlgorithmID : uint8_t {
  NULL_COMPRESSOR = 0,
  RLE_COMPRESSOR = 1,
  HUFFMAN_COMPRESSOR = 2,
  LZ77_COMPRESSOR = 3,
  BWT_COMPRESSOR = 4,
  ULTRA_COMPRESSOR = 5,
  EXTREME_COMPRESSOR = 6,
  OPTIMIZED_COMPRESSOR = 7,
  ARITHMETIC_COMPRESSOR = 8,
  PIPELINE_BWT_COMPRESSOR = 9, // BWT+MTF -> RLE -> Arithmetic (pipeline codec)
  ANS_COMPRESSOR = 10,         // static rANS entropy coder (pipeline codec)
  // Add future IDs here
  UNKNOWN = 255
};

constexpr size_t BASE_HEADER_SIZE =
    MAGIC_NUMBER.size() + sizeof(FORMAT_VERSION) + sizeof(AlgorithmID) +
    sizeof(uint64_t)    // Original Size
    + sizeof(uint32_t)  // Original Checksum (CRC32)
    + sizeof(uint32_t)  // Chunk Count
    + sizeof(uint32_t); // Chunk Size

// Backwards compatibility alias
constexpr size_t HEADER_SIZE = BASE_HEADER_SIZE;

// --- Header Structure (Conceptual) ---

// We won't use a packed struct directly to avoid portability issues (padding,
// endianness). Instead, we'll use serialization/deserialization functions.
struct FileHeader {
  // Magic number is implicitly checked/written
  uint8_t formatVersion = FORMAT_VERSION;
  AlgorithmID algorithmId = AlgorithmID::UNKNOWN;
  uint64_t originalSize = 0;
  uint32_t originalChecksum = 0; // Added CRC32 checksum
  uint32_t chunkCount = 1;
  uint32_t chunkSize = 0;
  std::vector<uint32_t> compressedSizes;
};

// --- Serialization / Deserialization ---
// Implemented out-of-line (src/format/FileFormat.cpp) on top of the
// FrameRegistry; byte layout is unchanged (little-endian, no padding).

/**
 * @brief Serializes the header data into a byte vector.
 * @param header The header data to serialize.
 * @return A vector of bytes representing the serialized header.
 */
std::vector<uint8_t> serializeHeader(const FileHeader &header);

/**
 * @brief Deserializes header data from a byte vector.
 * @param buffer The byte vector containing the serialized header (must be at
 * least HEADER_SIZE bytes).
 * @return The deserialized FileHeader.
 * @throws InvalidFormatError if the magic number is incorrect, or the buffer
 * is too small; UnsupportedVersionError if the format version is unknown;
 * CorruptDataError if the chunk metadata is malformed.
 */
FileHeader deserializeHeader(const std::vector<uint8_t> &buffer);

/**
 * @brief Maps AlgorithmID enum to a string representation.
 * @param id The AlgorithmID.
 * @return String name of the algorithm (e.g., "rle", "null").
 */
inline std::string algorithmIdToString(AlgorithmID id) {
  switch (id) {
  case AlgorithmID::NULL_COMPRESSOR:
    return "null";
  case AlgorithmID::RLE_COMPRESSOR:
    return "rle";
  case AlgorithmID::HUFFMAN_COMPRESSOR:
    return "huffman";
  case AlgorithmID::LZ77_COMPRESSOR:
    return "lz77";
  case AlgorithmID::BWT_COMPRESSOR:
    return "bwt";
  case AlgorithmID::ULTRA_COMPRESSOR:
    return "ultra";
  case AlgorithmID::EXTREME_COMPRESSOR:
    return "extreme";
  case AlgorithmID::OPTIMIZED_COMPRESSOR:
    return "optimized";
  case AlgorithmID::ARITHMETIC_COMPRESSOR:
    return "arithmetic";
  case AlgorithmID::PIPELINE_BWT_COMPRESSOR:
    return "bwt2";
  case AlgorithmID::ANS_COMPRESSOR:
    return "ans";
  default:
    return "unknown";
  }
}

/**
 * @brief Maps a string name to an AlgorithmID.
 * @param name The string name (e.g., "rle", "null").
 * @return The corresponding AlgorithmID, or UNKNOWN if not found.
 */
inline AlgorithmID stringToAlgorithmId(const std::string &name) {
  if (name == "null")
    return AlgorithmID::NULL_COMPRESSOR;
  if (name == "rle")
    return AlgorithmID::RLE_COMPRESSOR;
  if (name == "huffman")
    return AlgorithmID::HUFFMAN_COMPRESSOR;
  if (name == "lz77")
    return AlgorithmID::LZ77_COMPRESSOR;
  if (name == "bwt")
    return AlgorithmID::BWT_COMPRESSOR;
  if (name == "ultra")
    return AlgorithmID::ULTRA_COMPRESSOR;
  if (name == "extreme")
    return AlgorithmID::EXTREME_COMPRESSOR;
  if (name == "optimized")
    return AlgorithmID::OPTIMIZED_COMPRESSOR;
  if (name == "arithmetic")
    return AlgorithmID::ARITHMETIC_COMPRESSOR;
  if (name == "bwt2")
    return AlgorithmID::PIPELINE_BWT_COMPRESSOR;
  if (name == "ans")
    return AlgorithmID::ANS_COMPRESSOR;
  // Add mappings for future algorithms
  return AlgorithmID::UNKNOWN;
}

inline size_t serializedHeaderSize(const FileHeader &header) {
  return BASE_HEADER_SIZE + header.compressedSizes.size() * sizeof(uint32_t);
}

} // namespace format
} // namespace compression
