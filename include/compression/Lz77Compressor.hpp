#ifndef COMPRESSION_LZ77COMPRESSOR_HPP
#define COMPRESSION_LZ77COMPRESSOR_HPP

#include <cstddef> // Include for size_t

#include "ICompressor.hpp"
#include <vector>
#include <cstdint> // For uint types
#include <unordered_map> // For hash table optimization
#include <array>

namespace compression {

/**
 * @class Lz77Compressor
 * @brief Implements LZ77 compression algorithm with advanced optimizations
 * 
 * This implementation includes several optimizations:
 * - Optimal parsing using dynamic programming (when enabled)
 * - Enhanced hash function for better match distribution
 * - Adaptive encoding for different match lengths and distances
 * - Advanced match scoring that considers multiple factors
 * - Aggressive match finding with multi-position lookahead
 */
class Lz77Compressor : public ICompressor {
public:
    // Constants that need to be accessible by DeflateCompressor
    static constexpr uint8_t LITERAL_FLAG = 0x00;
    static constexpr uint8_t LENGTH_DISTANCE_FLAG = 0x01;
    static constexpr uint32_t EOB_SYMBOL = 256;
    static constexpr uint32_t LENGTH_CODE_BASE = 257;
    
    // Symbol structure for intermediate format
    struct Lz77Symbol {
        uint32_t symbol = 0;         // Value in the range [0, 285]
        size_t distance = 0;         // Distance for length-distance pairs
        size_t length = 0;           // Length for length-distance pairs
        uint8_t literal = 0;         // Literal value
        
        bool isLiteral() const { return symbol < 256; }
        bool isLength() const { return symbol >= 257 && symbol <= 285; }
        bool isEob() const { return symbol == EOB_SYMBOL; }
    };
    
    /**
     * @brief Construct a new Lz77Compressor object
     * 
     * @param windowSize Size of the search window
     * @param minMatchLength Minimum match length to consider
     * @param maxMatchLength Maximum match length to consider
     * @param useGreedyParsing When true, uses simple greedy parsing instead of lazy parsing
     * @param useOptimalParsing When true, uses optimal parsing for better compression ratio
     * @param aggressiveMatching When true, uses more aggressive match finding strategies
     */
    Lz77Compressor(
        size_t windowSize = 32768, 
        size_t minMatchLength = 3, 
        size_t maxMatchLength = 258,
        bool useGreedyParsing = false,
        bool useOptimalParsing = false,
        bool aggressiveMatching = true
    );
    
    /**
     * @brief Compress data using LZ77 algorithm
     * @param data Input data to compress
     * @return Compressed data as a vector of bytes
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
    
    /**
     * @brief Decompress LZ77-compressed data
     * @param data Compressed data to decompress
     * @return Decompressed data as a vector of bytes
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;
    
    /**
     * @brief Convert a length code to actual length
     * @param code The length code
     * @return The actual length
     */
    static uint32_t getLengthFromCode(uint32_t code);
    
private:
    // Configuration parameters
    size_t windowSize_;
    size_t minMatchLength_;
    size_t maxMatchLength_;
    bool useGreedyParsing_;
    bool useOptimalParsing_;
    bool aggressiveMatching_;
    
    // Hash table configuration
    size_t hashBits_ = 15;
    size_t maxHashChainLength_ = 64;
    size_t hashChainLimit_ = 8192;
    
    // Match structure with improved value calculation
    struct Match {
        size_t distance = 0;
        size_t length = 0;
        size_t position = 0;
        
        Match() = default;
        Match(size_t dist, size_t len, size_t pos = 0) 
            : distance(dist), length(len), position(pos) {}
        
        // Calculation of compression benefit in bytes
        float compressionBenefit() const {
            if (length < 3) return -1.0f;  // Minimum match length is usually 3
            
            // A match encoding typically costs 2-4 bytes (depending on length and distance)
            // While the literals it replaces would cost 1 byte each
            float encodingOverhead = 3.0f; // Average cost of length-distance pair
            return static_cast<float>(length) - encodingOverhead;
        }
    };
    
    // Enhanced hash function with better distribution
    uint32_t hashTriplet(const std::vector<uint8_t>& data, size_t pos) const;
    
    // Update hash table with better management for long chains
    void updateHashTable(std::unordered_map<uint32_t, std::vector<size_t>>& hashTable, 
                          const std::vector<uint8_t>& data, size_t pos) const;
    
    // Find best match with improved search strategy
    Match findBestMatchAt(const std::vector<uint8_t>& data, size_t pos, 
                         const std::unordered_map<uint32_t, std::vector<size_t>>& hashTable) const;
    
    // Advanced match scoring for better match selection
    float scoreMatch(const Match& match) const;
    
    // Get the length code for encoding
    uint32_t getLengthCode(size_t length) const;
    
    // Compress to intermediate symbol representation
    std::vector<Lz77Symbol> compressToSymbols(const std::vector<uint8_t>& data) const;
    
    // Encode symbols to bytes
    std::vector<uint8_t> encodeSymbols(const std::vector<Lz77Symbol>& symbols) const;
    
    // Optimal parsing using dynamic programming
    std::vector<Lz77Symbol> optimalParse(
        const std::vector<uint8_t>& data,
        std::unordered_map<uint32_t, std::vector<size_t>>& hashTable) const;
};

} // namespace compression

#endif // COMPRESSION_LZ77COMPRESSOR_HPP 