#ifndef COMPRESSION_LZ77COMPRESSOR_HPP
#define COMPRESSION_LZ77COMPRESSOR_HPP

#include <cstddef> // Include for std::byte, size_t

#include "ICompressor.hpp"
#include <vector>
#include <cstdint> // For uint types

namespace compression {

/**
 * @brief Implements the LZ77 compression algorithm.
 *
 * LZ77 works by finding repeated sequences of data and replacing them
 * with references (distance, length) to previous occurrences within a
 * sliding window.
 */
class Lz77Compressor : public ICompressor {
public:
    // --- Encoding constants --- 
    // These define the byte values used in the compressed stream
    static constexpr std::byte LITERAL_FLAG{0}; // Indicates the next byte is a literal
    static constexpr std::byte PAIR_FLAG{1};    // Indicates the next bytes are a (distance, length) pair

    /**
     * @brief Default constructor using typical window sizes.
     */
    Lz77Compressor(size_t searchBufferSize = 4096, size_t lookAheadBufferSize = 32);

    std::vector<std::byte> compress(const std::vector<std::byte>& data) const override;
    std::vector<std::byte> decompress(const std::vector<std::byte>& data) const override;

private:
    // Configuration for the sliding window
    const size_t searchBufferSize_;
    const size_t lookAheadBufferSize_;

    // Simple structure to represent a match found in the search buffer
    struct Match {
        size_t distance = 0; // How far back the match starts
        size_t length = 0;   // How long the match is
    };

    /**
     * @brief Finds the longest match for the lookahead buffer within the search buffer.
     * @param data The input data stream.
     * @param currentPosition The current position in the data stream (start of lookahead).
     * @param searchBufferStart The starting index of the search buffer.
     * @return Match The best match found (distance > 0 if a match was found).
     */
    Match findLongestMatch(const std::vector<std::byte>& data, size_t currentPosition, size_t searchBufferStart) const;

    // --- Encoding constants/helpers ---
    // We need a simple way to distinguish literals from (distance, length) pairs.
    // Example: Use a flag byte. 0 = literal, 1 = pair.
    // More sophisticated methods exist (e.g., prefix codes), but start simple.
    // Note: The exact encoding details will be refined during implementation.
    // Need to decide how to encode distance and length (e.g., fixed bytes? variable?)
};

} // namespace compression

#endif // COMPRESSION_LZ77COMPRESSOR_HPP 