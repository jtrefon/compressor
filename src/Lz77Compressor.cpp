#include "compression/Lz77Compressor.hpp"
#include <vector>
#include <stdexcept> // For potential errors
#include <algorithm> // For std::min
#include <cstring>   // For std::memcpy, memcmp (can optimize later)
#include <limits>    // For numeric_limits
#include <unordered_map> // Include for hash table
#include <functional> // For std::hash potentially, although direct int hash is fine

// Includes needed for hash chaining implementation
#include <vector> // For storing lists of positions

namespace {
    // A simple hash function for 3 consecutive bytes (triplet) - used for findBestMatchAt
    uint32_t hashTriplet(const uint8_t* data, size_t pos, size_t dataSize) {
        if (pos + 2 >= dataSize) {
            return 0; // Not enough data for valid hash
        }
        
        // Improved hash function with better distribution using the parameters from header
        uint32_t hash = (static_cast<uint32_t>(data[pos]) << 16) | 
                         (static_cast<uint32_t>(data[pos + 1]) << 8) | 
                         data[pos + 2];
        
        // More sophisticated mixing to improve hash distribution
        hash = hash * 2654435761U; // Knuth's multiplicative hash
        hash ^= hash >> compression::Lz77Compressor::HASH_SHIFT;
        hash &= compression::Lz77Compressor::HASH_MASK;
        
        return hash;
    }
    
    // Helper function to update the hash table with a position
    void updateHashTable(std::unordered_map<uint32_t, std::vector<size_t>>& hashTable,
                         const uint8_t* data, size_t pos, size_t dataSize,
                         size_t maxChainLength = compression::Lz77Compressor::MAX_HASH_CHAIN_LENGTH) {
        // Make sure we have enough data for a minimal match and hash
        if (pos + 2 >= dataSize) {
            return; // Not enough data
        }
        
        uint32_t hash = hashTriplet(data, pos, dataSize);
        auto& chain = hashTable[hash];
        
        // Add current position to the chain (more recent positions at the end)
        chain.push_back(pos);
        
        // Limit chain length to avoid excessive memory usage and search time
        if (chain.size() > maxChainLength) {
            chain.erase(chain.begin(), chain.begin() + (chain.size() - maxChainLength)); // Keep most recent positions
        }
    }
    
    // Helper to optimize string matching by comparing 4 bytes at once
    inline bool matchFourBytes(const uint8_t* a, const uint8_t* b) {
        return *reinterpret_cast<const uint32_t*>(a) == *reinterpret_cast<const uint32_t*>(b);
    }
}

namespace compression {

// --- Configuration & Constants ---
// Match constants should align with header definitions
constexpr size_t MIN_MATCH_LENGTH = Lz77Compressor::MIN_LEN;   // 3
constexpr size_t MAX_MATCH_LENGTH = Lz77Compressor::MAX_LEN;   // 258

// Max distance we can encode (2 bytes maximum for distance encoding)
constexpr uint16_t MAX_DISTANCE = 32768; 

// --- Flags for temporary byte-based compress() output ---
// These are only used in the temporary adapter compress() function
static constexpr uint8_t TEMP_LITERAL_FLAG = 0;
static constexpr uint8_t TEMP_PAIR_FLAG = 1;
static constexpr uint8_t TEMP_EOB_FLAG = 2;
static constexpr uint8_t TEMP_LITERAL_RUN_FLAG = 3;
static constexpr uint8_t TEMP_COMPACT_ENCODING_FLAG = 4;

// Constructor (align default lookahead with MAX_LEN)
Lz77Compressor::Lz77Compressor(size_t searchBufferSize, size_t lookAheadBufferSize, 
                               bool useGreedyParsing, bool useAdaptiveMinLength, 
                               bool aggressiveMatching) :
    searchBufferSize_(std::min(searchBufferSize, static_cast<size_t>(MAX_DISTANCE))),
    lookAheadBufferSize_(std::min(lookAheadBufferSize, MAX_MATCH_LENGTH)),
    useGreedyParsing_(useGreedyParsing),
    useAdaptiveMinLength_(useAdaptiveMinLength),
    aggressiveMatching_(aggressiveMatching)
{
    if (searchBufferSize_ == 0 || lookAheadBufferSize_ == 0) {
        throw std::invalid_argument("LZ77 buffer sizes cannot be zero.");
    }
    if (lookAheadBufferSize_ < MIN_MATCH_LENGTH) {
       throw std::invalid_argument("Lookahead buffer must be >= MIN_MATCH_LENGTH");
    }
}

// Extend a match by comparing bytes directly beyond the initial match
void Lz77Compressor::extendMatch(Match& match, const std::vector<uint8_t>& data, 
                               size_t currentPos, size_t matchPos) const {
    // Don't try to extend if we already have a full match
    if (match.length >= MAX_MATCH_LENGTH) {
        return;
    }
    
    // Bounds checking
    if (currentPos + match.length >= data.size() || 
        matchPos + match.length >= data.size()) {
        return;
    }
    
    // Determine maximum possible extension length
    size_t maxExtendLength = std::min({
        MAX_MATCH_LENGTH - match.length,
        data.size() - (currentPos + match.length),
        data.size() - (matchPos + match.length)
    });
    
    // Early exit if no extension possible
    if (maxExtendLength == 0) {
        return;
    }
    
    // Optimize for CPU cache by comparing in larger chunks when possible
    const uint8_t* pCurrent = data.data() + currentPos + match.length;
    const uint8_t* pMatch = data.data() + matchPos + match.length;
    
    size_t extraLength = 0;
    
    // Standard byte-by-byte exact matching (no mismatches allowed)
    // We've removed the aggressive matching with mismatches to ensure exact decompression
    while (extraLength < maxExtendLength && 
           pCurrent[extraLength] == pMatch[extraLength]) {
        extraLength++;
    }
    
    // Update match length with the extension
    match.length += extraLength;
}

// Helper function to find the best match at a given position using the hash table
Lz77Compressor::Match Lz77Compressor::findBestMatchAt(
    const std::vector<uint8_t>& data,
    size_t pos,
    const std::unordered_map<uint32_t, std::vector<size_t>>& hashTable) const
{
    Match bestMatch;
    bestMatch.length = 0;
    bestMatch.distance = 0;

    // Ensure we have enough data for a match
    if (pos + MIN_MATCH_LENGTH > data.size()) {
        return bestMatch; // Not enough data
    }

    uint32_t currentHash = hashTriplet(data.data(), pos, data.size());
    auto it = hashTable.find(currentHash);

    if (it != hashTable.end()) {
        const std::vector<size_t>& potentialPositions = it->second;
        size_t currentSearchBufferStart = (pos > searchBufferSize_) ? (pos - searchBufferSize_) : 0;
        size_t checksPerformed = 0;
        
        // Variables for best match tracking with improved scoring
        float bestScore = 0.0f;
        Match secondBestMatch; // Track second best match for more informed decisions
        float secondBestScore = 0.0f;
        
        // Increase chain length for aggressive matching
        size_t maxPositionsToCheck = aggressiveMatching_ ? MAX_HASH_CHAIN_LENGTH : (MAX_HASH_CHAIN_LENGTH / 2);
        
        // Iterate backwards through potential positions (most recent first)
        for (auto rit = potentialPositions.rbegin(); 
             rit != potentialPositions.rend() && checksPerformed < maxPositionsToCheck; 
             ++rit, ++checksPerformed) {
            
            size_t potentialMatchPos = *rit;
            if (potentialMatchPos < currentSearchBufferStart) break; // Outside window
            if (potentialMatchPos >= pos) continue; // Cannot match self or future

            // Calculate distance
            size_t distance = pos - potentialMatchPos;
            
            // Skip positions if they're too far (greater than our encoding limit)
            if (distance > MAX_DISTANCE || distance == 0) {
                continue;
            }
            
            // Apply adaptive minimum match length based on distance
            size_t minMatchLengthForDistance = getMinMatchLength(distance);

            // Calculate initial match length - must be at least MIN_MATCH_LENGTH
            size_t currentMatchLength = 0;
            const size_t maxPossibleLength = std::min({
                lookAheadBufferSize_, 
                MAX_MATCH_LENGTH, 
                data.size() - pos
            });
            
            // Verify initial triplet with bounds checking
            if (potentialMatchPos + 2 < data.size() &&
                pos + 2 < data.size() &&
                data[potentialMatchPos] == data[pos] && 
                data[potentialMatchPos + 1] == data[pos + 1] && 
                data[potentialMatchPos + 2] == data[pos + 2]) {
                
                // Start at 3 since we already confirmed the triplet
                currentMatchLength = 3;
                
                // Extend match beyond the initial triplet
                size_t i = 3;
                while (currentMatchLength < maxPossibleLength &&
                       potentialMatchPos + i < data.size() &&
                       pos + i < data.size() &&
                       data[potentialMatchPos + i] == data[pos + i]) {
                    currentMatchLength++;
                    i++;
                }
            }

            // Evaluate if this match is better than our current best and meets minimum length
            if (currentMatchLength >= minMatchLengthForDistance) {
                Match candidateMatch;
                candidateMatch.length = currentMatchLength;
                candidateMatch.distance = distance;
                
                // Only consider this match if it's worth using based on compression benefit
                if (isMatchWorthUsing(candidateMatch)) {
                    // Calculate match score using the new scoring function
                    float matchScore = scoreMatch(candidateMatch);
                    
                    // If this match is better than our current best
                    if (bestMatch.length == 0 || matchScore > bestScore) {
                        // Move current best to second best
                        secondBestMatch = bestMatch;
                        secondBestScore = bestScore;
                        
                        // Update best match
                        bestMatch = candidateMatch;
                        bestScore = matchScore;
                        
                        // If we found a very good match, we might stop early
                        if (bestMatch.length >= (MAX_MATCH_LENGTH * 0.85) && !aggressiveMatching_) {
                            break;
                        }
                    }
                    // Keep track of the second best match
                    else if (secondBestMatch.length == 0 || matchScore > secondBestScore) {
                        secondBestMatch = candidateMatch;
                        secondBestScore = matchScore;
                    }
                }
            }
            
            // With aggressive matching, continue searching even after finding a good match
            if (!aggressiveMatching_ && checksPerformed > 32 && bestMatch.length >= MIN_MATCH_LENGTH * 2) {
                break; // Exit early for performance if we found a reasonably good match
            }
        }
    }
    
    // Try to extend the best match if it's worth extending
    if (bestMatch.length >= MIN_MATCH_LENGTH) {
        extendMatch(bestMatch, data, pos, pos - bestMatch.distance);
    }
    
    return bestMatch;
}

// New function: Generates LZ77 symbols
std::vector<Lz77Compressor::Lz77Symbol> Lz77Compressor::compressToSymbols(const std::vector<uint8_t>& data) const {
    if (data.empty()) return {};

    std::vector<Lz77Symbol> symbols; 
    symbols.reserve(data.size() / 2); // Conservative estimate to avoid frequent reallocations
    std::unordered_map<uint32_t, std::vector<size_t>> hashTable;
    
    // Reserve space in hash table to avoid rehashing - increased capacity for better hashing
    hashTable.reserve(16384);
    
    // Build the initial hash table with a reasonable amount of positions
    // Increased preload for better compression at the start of the file
    const size_t preloadLimit = std::min(size_t(8192), data.size());
    for (size_t i = 0; i + MIN_MATCH_LENGTH <= preloadLimit; i++) {
        updateHashTable(hashTable, data.data(), i, data.size(), MAX_HASH_CHAIN_LENGTH);
    }
    
    size_t currentPosition = 0;

    // Main compression loop
    while (currentPosition < data.size()) {
        // Find the best match at the current position
        Match match = findBestMatchAt(data, currentPosition, hashTable);
        
        // Lazy matching: check if skipping one byte gives better compression
        Match nextMatch;
        bool useLazyMatch = false;
        
        if (!useGreedyParsing_ && match.length >= MIN_MATCH_LENGTH && 
            currentPosition + 1 < data.size() && 
            match.compressionBenefit() > 0) {
            
            // Try to find a match at the next position
            nextMatch = findBestMatchAt(data, currentPosition + 1, hashTable);
            
            // Use lazy match if it's better by a significant margin - improved criteria
            if (nextMatch.length > match.length && 
                (nextMatch.compressionBenefit() > match.compressionBenefit() + 1 || 
                (aggressiveMatching_ && scoreMatch(nextMatch) > scoreMatch(match) * 1.1f))) {
                useLazyMatch = true;
            }
        }
        
        // With aggressive matching, also check for better matches by skipping 2 bytes
        if (aggressiveMatching_ && match.length >= MIN_MATCH_LENGTH && 
            currentPosition + 2 < data.size() && !useLazyMatch) {
            
            Match nextNextMatch = findBestMatchAt(data, currentPosition + 2, hashTable);
            
            // Use two-byte lazy match if it's significantly better
            if (nextNextMatch.length > match.length + 1 && 
                nextNextMatch.compressionBenefit() > match.compressionBenefit() + 3) {
                // Output two literals and then use the match later
                for (int i = 0; i < 2 && currentPosition < data.size(); i++) {
                    Lz77Symbol symbol;
                    symbol.symbol = static_cast<uint32_t>(data[currentPosition]);
                    symbol.literal = data[currentPosition];
                    symbols.push_back(symbol);
                    
                    if (currentPosition + MIN_MATCH_LENGTH <= data.size()) {
                        updateHashTable(hashTable, data.data(), currentPosition, data.size());
                    }
                    
                    currentPosition++;
                }
                
                continue; // Skip to next iteration to handle the better match
            }
        }

        // Update hash table for current position
        if (currentPosition + MIN_MATCH_LENGTH <= data.size()) {
            updateHashTable(hashTable, data.data(), currentPosition, data.size());
        }

        if (match.length >= MIN_MATCH_LENGTH && match.compressionBenefit() > 0 && !useLazyMatch) {
            // Output a length-distance pair
            size_t actualLength = std::min(match.length, MAX_MATCH_LENGTH);
            uint32_t lengthCode = getLengthCode(actualLength);
            
            Lz77Symbol symbol;
            symbol.symbol = lengthCode;
            symbol.distance = match.distance;
            symbol.length = actualLength;
            symbols.push_back(symbol);
            
            // Update hash table for skipped positions more frequently
            // Use a smaller stride to update the hash table more thoroughly
            const size_t stride = aggressiveMatching_ ? 
                                  std::max(size_t(2), actualLength / 16) : 
                                  std::max(size_t(4), actualLength / 8);
                                  
            for (size_t i = 1; i < actualLength; i += stride) {
                if (currentPosition + i + MIN_MATCH_LENGTH <= data.size()) {
                    updateHashTable(hashTable, data.data(), currentPosition + i, data.size());
                }
            }
            
            currentPosition += actualLength;
        } else if (useLazyMatch) {
            // Output current byte as literal, then we'll use the better match next
            Lz77Symbol symbol;
            symbol.symbol = static_cast<uint32_t>(data[currentPosition]);
            symbol.literal = data[currentPosition];
            symbols.push_back(symbol);
            currentPosition++;
        } else {
            // Output current byte as literal
            if (currentPosition < data.size()) {
                Lz77Symbol symbol;
                symbol.symbol = static_cast<uint32_t>(data[currentPosition]);
                symbol.literal = data[currentPosition];
                symbols.push_back(symbol);
                currentPosition++;
            } else {
                break;
            }
        }
    }

    // End with EOB symbol
    Lz77Symbol eob;
    eob.symbol = EOB_SYMBOL;
    symbols.push_back(eob);

    return symbols;
}

// Override of ICompressor compress method
std::vector<uint8_t> Lz77Compressor::compress(const std::vector<uint8_t>& data) const {
    // Handle empty input
    if (data.empty()) {
        return {};
    }
    
    // First obtain LZ77 symbols
    std::vector<Lz77Symbol> symbols = compressToSymbols(data);
    
    // Estimate a reasonable output size
    std::vector<uint8_t> result;
    result.reserve(data.size());
    
    // Process symbols and encode to output - with optimized encoding
    size_t i = 0;
    while (i < symbols.size()) {
        if (symbols[i].symbol == EOB_SYMBOL) {
            // End of block marker
            result.push_back(TEMP_EOB_FLAG);
            i++;
            continue;
        }
        
        // Check for consecutive literals
        if (symbols[i].isLiteral()) {
            // Count consecutive literals
            size_t literalCount = 0;
            size_t j = i;
            while (j < symbols.size() && symbols[j].isLiteral() && literalCount < 255) {
                literalCount++;
                j++;
            }
            
            // For runs of 2+ literals, use the optimized run encoding (changed from 3+ for better compression)
            if (literalCount >= 2) {
                result.push_back(TEMP_LITERAL_RUN_FLAG); // Special marker for literal runs
                result.push_back(literalCount - 1); // 0-254 means 1-255 literals
                
                // Add all literals
                for (size_t k = 0; k < literalCount && i + k < symbols.size(); k++) {
                    result.push_back(symbols[i + k].literal);
                }
                
                i += literalCount;
            } else {
                // Handle single literal with the standard encoding
                result.push_back(TEMP_LITERAL_FLAG);
                result.push_back(symbols[i].literal);
                i++;
            }
        } else {
            // Length/distance pair - optimize encoding by recognizing common patterns
            // Check if this is a short match at a close distance
            if (symbols[i].length <= 6 && symbols[i].distance < 1024 && symbols[i].distance > 0) {
                // Compact encoding for small distances and lengths
                // Format: [4] [LLLLLLDD DDDDDDDD]
                // Where L = length bits, D = distance bits
                // 6 bits for length (3-6), 10 bits for distance (1-1023)
                
                uint16_t encodedValue = ((symbols[i].length - 3) << 10) | (symbols[i].distance & 0x3FF);
                result.push_back(TEMP_COMPACT_ENCODING_FLAG); // Special marker for compact encoding
                result.push_back(encodedValue & 0xFF);
                result.push_back((encodedValue >> 8) & 0xFF);
                
                i++;
            } else {
                // Standard encoding for larger lengths/distances
                result.push_back(TEMP_PAIR_FLAG);
                
                // Length (16 bits)
                result.push_back(symbols[i].length & 0xFF);
                result.push_back((symbols[i].length >> 8) & 0xFF);
                
                // Distance (16 bits)
                result.push_back(symbols[i].distance & 0xFF);
                result.push_back((symbols[i].distance >> 8) & 0xFF);
                
                i++;
            }
        }
    }
    
    return result;
}

// Override of ICompressor decompress method
std::vector<uint8_t> Lz77Compressor::decompress(const std::vector<uint8_t>& data) const {
    // Handle empty input
    if (data.empty()) {
        return {};
    }
    
    std::vector<uint8_t> result;
    result.reserve(data.size() * 2); // Rough estimate
    
    // Process input byte by byte
    size_t pos = 0;
    while (pos < data.size()) {
        // Check bounds before reading flag
        if (pos >= data.size()) {
            throw std::runtime_error("Unexpected end of data (flag)");
        }
        
        uint8_t flag = data[pos++];
        
        if (flag == TEMP_LITERAL_FLAG) {
            // Single literal
            if (pos >= data.size()) {
                throw std::runtime_error("Unexpected end of data (literal)");
            }
            
            result.push_back(data[pos++]);
        } else if (flag == TEMP_PAIR_FLAG) {
            // Length/distance pair
            if (pos + 3 >= data.size()) {
                throw std::runtime_error("Unexpected end of data (length/distance)");
            }
            
            // Length (16 bits)
            uint16_t length = data[pos] | (static_cast<uint16_t>(data[pos + 1]) << 8);
            pos += 2;
            
            // Distance (16 bits)
            uint16_t distance = data[pos] | (static_cast<uint16_t>(data[pos + 1]) << 8);
            pos += 2;
            
            // Validate distance
            if (distance == 0 || distance > result.size()) {
                throw std::runtime_error("Invalid distance (beyond output size or zero)");
            }
            
            // Copy from earlier position (the match) - support overlapping
            size_t startPos = result.size() - distance;
            for (size_t i = 0; i < length; i++) {
                result.push_back(result[startPos + i]);
            }
        } else if (flag == TEMP_EOB_FLAG) {
            // EOB - end of block/stream
            break;
        } else if (flag == TEMP_LITERAL_RUN_FLAG) {
            // Literal run
            if (pos >= data.size()) {
                throw std::runtime_error("Unexpected end of data (literal run count)");
            }
            
            // Read literal count
            uint8_t count = data[pos++] + 1; // +1 because we stored count-1
            
            if (pos + count > data.size()) {
                throw std::runtime_error("Unexpected end of data (literal run data)");
            }
            
            // Copy all literals
            for (uint8_t i = 0; i < count; i++) {
                result.push_back(data[pos++]);
            }
        } else if (flag == TEMP_COMPACT_ENCODING_FLAG) {
            // Compact encoding for small distances and lengths
            if (pos + 1 >= data.size()) {
                throw std::runtime_error("Unexpected end of data (compact encoding)");
            }
            
            // Read encoded value
            uint16_t encodedValue = data[pos] | (static_cast<uint16_t>(data[pos + 1]) << 8);
            pos += 2;
            
            // Extract length and distance
            uint16_t length = ((encodedValue >> 10) & 0x3F) + 3; // 6 bits for length
            uint16_t distance = encodedValue & 0x3FF; // 10 bits for distance
            
            // Validate distance
            if (distance == 0 || distance > result.size()) {
                throw std::runtime_error("Invalid distance (beyond output size or zero)");
            }
            
            // Copy from earlier position (the match) - support overlapping
            size_t startPos = result.size() - distance;
            for (size_t i = 0; i < length; i++) {
                result.push_back(result[startPos + i]);
            }
        } else {
            throw std::runtime_error("Invalid flag in compressed data: " + std::to_string(static_cast<int>(flag)));
        }
    }
    
    return result;
}

} // namespace compression 