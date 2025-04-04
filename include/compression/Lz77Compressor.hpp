#ifndef COMPRESSION_LZ77COMPRESSOR_HPP
#define COMPRESSION_LZ77COMPRESSOR_HPP

#include <cstddef> // Include for size_t

#include "ICompressor.hpp"
#include <vector>
#include <cstdint> // For uint types
#include <unordered_map> // For hash table optimization

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
    // Constants for LZ77 symbols (inspired by Deflate)
    static constexpr uint32_t EOB_SYMBOL = 256;
    static constexpr uint32_t LENGTH_CODE_BASE = 257;
    static constexpr uint32_t MIN_LEN = 3; // Corresponds to MIN_MATCH_LENGTH
    static constexpr uint32_t MAX_LEN = 258; // Corresponds to MAX_MATCH_LENGTH
    
    // Constants for improved compression
    static constexpr uint32_t MAX_HASH_CHAIN_LENGTH = 8192; // Maximum positions to check in hash chain
    static constexpr bool USE_HASH_CHAIN_LIMIT = true; // Enable hash chain limit for speed vs compression trade-off
    static constexpr size_t MIN_MATCH_FOR_FAR_DISTANCE = 5; // Require longer matches for far distances
    
    // Constants for adaptive minimum match length
    static constexpr size_t ADAPTIVE_MIN_LENGTH_CLOSE = 3;  // Min length for nearby matches (< 4K)
    static constexpr size_t ADAPTIVE_MIN_LENGTH_FAR = 4;    // Min length for 4-16K distances
    static constexpr size_t ADAPTIVE_MIN_LENGTH_VERY_FAR = 5; // Min length for 16K+ distances

    // Helper to get length code from actual length
    static uint32_t getLengthCode(size_t length) {
        if (length < MIN_LEN || length > MAX_LEN) {
            // Handle error or clamp - for now, maybe throw?
            // Or ensure this is never called with invalid lengths
            return 0; // Indicate error/invalid
        }
        return LENGTH_CODE_BASE + (static_cast<uint32_t>(length) - MIN_LEN);
    }

    // Helper to get length from code
    static size_t getLengthFromCode(uint32_t code) {
        if (code < LENGTH_CODE_BASE || code >= (LENGTH_CODE_BASE + (MAX_LEN - MIN_LEN + 1))) {
            return 0; // Indicate error/invalid
        }
        return static_cast<size_t>(code - LENGTH_CODE_BASE + MIN_LEN);
    }

    // Note: Distance codes/handling will be added later

    /**
     * @brief Default constructor using typical window sizes.
     * 
     * @param searchBufferSize Size of the search buffer (increased for better compression)
     * @param lookAheadBufferSize Size of the look-ahead buffer
     * @param useGreedyParsing When true, uses greedy parsing instead of lazy parsing
     */
    Lz77Compressor(size_t searchBufferSize = 32768, size_t lookAheadBufferSize = 258, bool useGreedyParsing = false, 
                  bool useAdaptiveMinLength = true);

    // Generate a sequence of symbols (Literals 0-255, EOB 256, Length Codes 257+)
    // and associated distances for length codes.
    // We might need a struct to return both symbols and distances.
    struct Lz77Symbol {
        uint32_t symbol;    // Literal (0-255), EOB (256), or Length Code (257+)
        uint32_t distance = 0;  // Distance for length codes (0 if literal)
        uint32_t length = 0;    // Actual length (3-258) for length codes (0 if literal)
        uint8_t literal = 0; // Actual literal byte (only if symbol <= 255)

        bool isLiteral() const { return symbol < LENGTH_CODE_BASE; }
    };

    std::vector<Lz77Symbol> compressToSymbols(const std::vector<uint8_t>& data) const;

    // Original compress/decompress might be removed or adapted later
    // They don't fit the Deflate model directly anymore.
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;

private:
    // Configuration for the sliding window
    size_t searchBufferSize_;
    size_t lookAheadBufferSize_;
    bool useGreedyParsing_; // Greedy vs lazy parsing
    bool useAdaptiveMinLength_; // Dynamically adjust min match length based on distance

    // Simple structure to represent a match found in the search buffer
    struct Match {
        size_t distance = 0; // How far back the match starts
        size_t length = 0;   // How long the match is
        
        // Calculate compression benefit (bytes saved)
        int compressionBenefit() const {
            // Standard encoding:
            // - A match costs 5 bytes (flag + 2 bytes length + 2 bytes distance)
            // - A literal costs 2 bytes (flag + literal)
            
            // Calculate savings
            int literalCost = static_cast<int>(length) * 2;  // Cost if encoded as literals
            int matchCost = 5;  // Fixed cost of encoding match
            
            // Calculate benefit (positive means savings)
            return literalCost - matchCost;
        }
    };

    // Helper method to determine minimum match length based on distance
    size_t getMinMatchLength(size_t distance) const {
        if (!useAdaptiveMinLength_)
            return MIN_LEN;
            
        if (distance < 4096)
            return ADAPTIVE_MIN_LENGTH_CLOSE;
        else if (distance < 16384)
            return ADAPTIVE_MIN_LENGTH_FAR;
        else
            return ADAPTIVE_MIN_LENGTH_VERY_FAR;
    }

    // Helper for optimized compression logic
    Match findBestMatchAt(
        const std::vector<uint8_t>& data,
        size_t pos,
        const std::unordered_map<uint32_t, std::vector<size_t>>& hashTable) const;
        
    // Attempt to extend a match by comparing bytes directly, bypassing hash lookup
    void extendMatch(Match& match, const std::vector<uint8_t>& data, size_t currentPos, size_t matchPos) const;
        
    // Evaluate if a match is worth using based on distance and length
    bool isMatchWorthUsing(const Match& match, size_t distance = 0) const {
        size_t dist = (distance > 0) ? distance : match.distance;
        
        // Always require positive compression benefit
        if (match.compressionBenefit() <= 0) return false;
        
        // For far distances, require longer matches based on adaptive minimum length
        size_t minRequiredLength = getMinMatchLength(dist);
        if (match.length < minRequiredLength) {
            return false;
        }
        
        // Additional check for very short matches
        if (match.length == MIN_LEN && dist > 8192) {
            return false; // Minimum-length matches only for close distances
        }
        
        return true;
    }

    // --- Encoding constants/helpers ---
    // We need a simple way to distinguish literals from (distance, length) pairs.
    // Example: Use a flag byte. 0 = literal, 1 = pair.
    // More sophisticated methods exist (e.g., prefix codes), but start simple.
    // Note: The exact encoding details will be refined during implementation.
    // Need to decide how to encode distance and length (e.g., fixed bytes? variable?)
};

} // namespace compression

#endif // COMPRESSION_LZ77COMPRESSOR_HPP 