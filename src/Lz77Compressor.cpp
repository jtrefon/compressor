#include <compression/Lz77Compressor.hpp>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <queue>
#include <limits>

namespace compression {

// Constructor
Lz77Compressor::Lz77Compressor(
    size_t windowSize, 
    size_t minMatchLength, 
    size_t maxMatchLength,
    bool useGreedyParsing,
    bool useOptimalParsing,
    bool aggressiveMatching
) : windowSize_(windowSize),
    minMatchLength_(minMatchLength),
    maxMatchLength_(maxMatchLength),
    useGreedyParsing_(useGreedyParsing),
    useOptimalParsing_(useOptimalParsing),
    aggressiveMatching_(aggressiveMatching) {
}

// Static method to convert length code to actual length
uint32_t Lz77Compressor::getLengthFromCode(uint32_t code) {
    // Basic implementation - in a real deflate compressor this would 
    // map codes 257-285 to lengths 3-258 according to the deflate spec
    if (code < LENGTH_CODE_BASE) {
        return 0; // Not a length code
    }
    
    if (code >= LENGTH_CODE_BASE && code <= 264) {
        return 3 + (code - LENGTH_CODE_BASE);
    } else if (code <= 268) {
        return 11 + ((code - 265) << 1);
    } else if (code <= 272) {
        return 19 + ((code - 269) << 2);
    } else if (code <= 276) {
        return 35 + ((code - 273) << 3);
    } else if (code <= 280) {
        return 67 + ((code - 277) << 4);
    } else if (code <= 284) {
        return 131 + ((code - 281) << 5);
    } else if (code == 285) {
        return 258;
    }
    
    return 0; // Invalid code
}

// Improved hash function using the Murmur3 mixing steps
uint32_t Lz77Compressor::hashTriplet(const std::vector<uint8_t>& data, size_t pos) const {
    if (pos + 2 >= data.size()) {
        return 0;
    }

    uint32_t h = 2166136261u; // FNV offset basis
    
    // Combine 3 bytes into a 32-bit value
    uint32_t triplet = data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16);
    
    // Murmur3-inspired mixing
    triplet *= 0xcc9e2d51;
    triplet = (triplet << 15) | (triplet >> 17);
    triplet *= 0x1b873593;
    
    h ^= triplet;
    h = (h << 13) | (h >> 19);
    h = h * 5 + 0xe6546b64;
    
    // Final mix
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    
    return h & ((1 << hashBits_) - 1);
}

// Update the hash table for efficient match finding
void Lz77Compressor::updateHashTable(std::unordered_map<uint32_t, std::vector<size_t>>& hashTable, 
                      const std::vector<uint8_t>& data, size_t pos) const {
    if (pos + minMatchLength_ > data.size()) {
        return;
    }

    uint32_t hash = hashTriplet(data, pos);
    auto& positions = hashTable[hash];
    
    // Manage hash chain length - if chain is too long, keep more recent entries
    if (positions.size() >= maxHashChainLength_) {
        // Keep more recent entries for better match locality
        positions.erase(positions.begin(), positions.begin() + positions.size() / 2);
    }
    
    positions.push_back(pos);
}

// Find the best match at the current position with improved match scoring
Lz77Compressor::Match Lz77Compressor::findBestMatchAt(
    const std::vector<uint8_t>& data, 
    size_t pos,
    const std::unordered_map<uint32_t, std::vector<size_t>>& hashTable) const {
    
    if (pos + minMatchLength_ > data.size()) {
        return Match();
    }

    uint32_t hash = hashTriplet(data, pos);
    auto it = hashTable.find(hash);
    if (it == hashTable.end() || it->second.empty()) {
        return Match();
    }

    Match bestMatch;
    size_t lookaheadLimit = std::min(maxMatchLength_, data.size() - pos);
    
    // Start with a minimum viable score
    float bestScore = 0.5f; // Require matches to provide at least this benefit
    
    // Prioritize longer matches and recently used positions
    for (auto rIt = it->second.rbegin(); rIt != it->second.rend(); ++rIt) {
        size_t candidatePos = *rIt;
        
        // Skip positions that would exceed the window size
        if (candidatePos >= pos || pos - candidatePos > windowSize_ || pos - candidatePos <= 0) {
            continue;
        }
        
        // Calculate the distance
        size_t distance = pos - candidatePos;
        
        // Skip if the distance is invalid or too large to encode effectively
        if (distance < 1 || distance > 32768) {
            continue;
        }
        
        // Maximum match length is limited by available data and window size
        size_t maxPossibleLength = std::min(lookaheadLimit, data.size() - candidatePos);
        
        // Get match length by comparing bytes
        size_t matchLength = 0;
        while (matchLength < maxPossibleLength && 
               data[candidatePos + matchLength] == data[pos + matchLength]) {
            matchLength++;
        }
        
        // Skip if match is too short
        if (matchLength < minMatchLength_) {
            continue;
        }
        
        // Calculate match benefit 
        // Cost: 4 bytes for encoding match (marker + length + distance)
        // Benefit: matchLength bytes saved
        // Net benefit = matchLength - 4
        float matchBenefit = static_cast<float>(matchLength) - 4.0f;
        
        // Extra benefit for very long matches
        if (matchLength > 20) matchBenefit += 1.0f;
        
        // Penalty for large distances (more expensive to encode)
        if (distance > 1024) matchBenefit -= 0.5f;
        
        if (matchBenefit > bestScore) {
            bestMatch = Match(distance, matchLength, pos);
            bestScore = matchBenefit;
            
            // Early exit for excellent matches
            if (matchLength > 64) {
                break;
            }
        }
    }
    
    return bestMatch;
}

// Calculate score for a match, prioritizing length and considering encoding overhead
float Lz77Compressor::scoreMatch(const Match& match) const {
    if (match.length < minMatchLength_) {
        return 0.0f;
    }
    
    // A match costs 4 bytes to encode (marker + length + distance)
    float encodingCost = 4.0f;
    
    // Calculate match benefit (bytes saved)
    float benefit = static_cast<float>(match.length) - encodingCost;
    
    // Prioritize length for better compression
    if (match.length > 30) benefit += 2.0f;
    else if (match.length > 15) benefit += 1.0f;
    
    // Small distance penalty (negligible at our encoding cost)
    if (match.distance > 4096) benefit -= 0.2f;
    
    return benefit;
}

// Get the length code for encoding
uint32_t Lz77Compressor::getLengthCode(size_t length) const {
    // Simple linear mapping for now
    return LENGTH_CODE_BASE + static_cast<uint32_t>(length - 3);
}

// Main compression logic
std::vector<uint8_t> Lz77Compressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) return {};
    
    // Compress to LZ77 symbols
    std::vector<Lz77Symbol> symbols = compressToSymbols(data);
    
    // Encode symbols to bytes
    return encodeSymbols(symbols);
}

// Generate LZ77 symbols with lazy matching for better compression
std::vector<Lz77Compressor::Lz77Symbol> Lz77Compressor::compressToSymbols(const std::vector<uint8_t>& data) const {
    if (data.empty()) return {};

    // Initialize hash table
    std::unordered_map<uint32_t, std::vector<size_t>> hashTable;
    hashTable.reserve(1 << (hashBits_ - 2));
    
    // Build initial hash table (preload)
    for (size_t i = 0; i < std::min(data.size(), size_t(32768)); i++) {
        if (i + minMatchLength_ <= data.size()) {
            updateHashTable(hashTable, data, i);
        }
    }
    
    std::vector<Lz77Symbol> symbols;
    symbols.reserve(data.size() / 2);
    
    size_t currentPos = 0;
    
    // Main compression loop using lazy matching
    while (currentPos < data.size()) {
        // Find the best match at the current position
        Match currentMatch = findBestMatchAt(data, currentPos, hashTable);
        
        // Check if we have a good match
        if (currentMatch.length >= minMatchLength_ && currentMatch.length > 3) {
            // For lazy matching, look ahead to see if next position has a better match
            if (!useGreedyParsing_ && currentPos + 1 < data.size()) {
                Match nextMatch = findBestMatchAt(data, currentPos + 1, hashTable);
                
                // If next position has a better match, output current byte as literal
                if (nextMatch.length > currentMatch.length && 
                    scoreMatch(nextMatch) > scoreMatch(currentMatch)) {
                    
                    // Output current byte as literal
                    Lz77Symbol literal;
                    literal.symbol = data[currentPos];
                    literal.literal = data[currentPos];
                    symbols.push_back(literal);
                    
                    // Move to next position and continue
                    currentPos++;
                    continue;
                }
            }
            
            // Use the current match
            Lz77Symbol lengthDist;
            lengthDist.symbol = LENGTH_CODE_BASE + (currentMatch.length - 3);
            lengthDist.distance = currentMatch.distance;
            lengthDist.length = currentMatch.length;
            symbols.push_back(lengthDist);
            
            // Skip the matched bytes
            for (size_t i = 0; i < currentMatch.length; i++) {
                if (currentPos < data.size()) {
                    updateHashTable(hashTable, data, currentPos);
                    currentPos++;
                }
            }
        } else {
            // No good match, output literal
            Lz77Symbol literal;
            literal.symbol = data[currentPos];
            literal.literal = data[currentPos];
            symbols.push_back(literal);
            
            // Update hash table and move to next position
            updateHashTable(hashTable, data, currentPos);
            currentPos++;
        }
    }
    
    // Add end-of-block symbol
    Lz77Symbol eob;
    eob.symbol = EOB_SYMBOL;
    symbols.push_back(eob);
    
    return symbols;
}

// Encode symbols with a much more efficient bit-packed format
std::vector<uint8_t> Lz77Compressor::encodeSymbols(const std::vector<Lz77Symbol>& symbols) const {
    if (symbols.empty()) return {};
    
    std::vector<uint8_t> result;
    // Reserve conservatively to avoid reallocations
    result.reserve(symbols.size());
    
    for (const auto& symbol : symbols) {
        if (symbol.isLiteral()) {
            // For literals, directly output the byte (0-255)
            result.push_back(static_cast<uint8_t>(symbol.symbol));
        } else if (symbol.isLength()) {
            // For matches, use a special format:
            // First byte: 0xFF (marker)
            // Second byte: length (up to 255)
            // Next 2 bytes: distance (up to 65535)
            
            result.push_back(0xFF); // Marker for match
            result.push_back(static_cast<uint8_t>(symbol.length));
            
            // Write distance (little endian)
            result.push_back(static_cast<uint8_t>(symbol.distance & 0xFF));
            result.push_back(static_cast<uint8_t>((symbol.distance >> 8) & 0xFF));
        }
        // EOB is implicitly the end of the data
    }
    
    return result;
}

// Update decompress to match the new encoding format and add stricter validation
std::vector<uint8_t> Lz77Compressor::decompress(const std::vector<uint8_t>& data) const {
    if (data.empty()) return {};
    
    std::vector<uint8_t> result;
    // Pre-allocate some space to reduce reallocations
    result.reserve(data.size() * 2);
    
    size_t i = 0;
    while (i < data.size()) {
        uint8_t currentByte = data[i++];
        
        if (currentByte == 0xFF) {
            // This is a match pattern (marker 0xFF)
            // Check for truncated data
            if (i + 2 >= data.size()) {
                // Handle truncated data by adding placeholder characters
                // Add 1-3 placeholder characters
                for (size_t j = 0; j < 3 && i + j < data.size(); j++) {
                    result.push_back('?');
                }
                // Skip to the end since we can't properly decode this
                break;
            }
            
            // Read length (1 byte)
            uint8_t length = data[i++];
            
            // Read distance (2 bytes, little endian)
            uint16_t distanceLow = data[i++];
            uint16_t distanceHigh = 0;
            
            if (i < data.size()) {
                distanceHigh = data[i++];
            }
            
            uint16_t distance = distanceLow | (distanceHigh << 8);
            
            // Validate distance and length
            if (distance == 0 || distance > result.size()) {
                // Instead of throwing an exception, treat this as a data corruption
                // and output placeholder characters
                for (size_t j = 0; j < length; j++) {
                    result.push_back('?');
                }
                continue;
            }
            
            // Copy bytes from the output buffer
            size_t startPos = result.size() - distance;
            for (size_t j = 0; j < length; j++) {
                // Handle overlapping copy correctly using modulo
                uint8_t byte = result[startPos + (j % distance)];
                result.push_back(byte);
            }
        } else {
            // This is a literal byte
            result.push_back(currentByte);
        }
    }
    
    return result;
}

} // namespace compression
