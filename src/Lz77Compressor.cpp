#include <compression/Lz77Compressor.hpp>
#include <vector>
#include <stdexcept> // For potential errors
#include <algorithm> // For std::min
#include <cstring>   // For std::memcpy, memcmp (can optimize later)
#include <limits>    // For numeric_limits
#include <unordered_map> // Include for hash table
#include <functional> // For std::hash potentially, although direct int hash is fine

namespace compression {

// --- Configuration & Constants ---
// Minimum length for a match to be encoded as a pair (shorter sequences are literals)
// Must be at least 3, otherwise encoding [flag, dist, len] = 4 bytes is worse than
// encoding 2 literals = [flag, lit] + [flag, lit] = 4 bytes.
// Encoding 3 literals = 6 bytes. Pair = 4 bytes. So min length 3 makes sense.
constexpr size_t MIN_MATCH_LENGTH = 3;

// Max distance we can encode (relative to search buffer size and encoding scheme)
// If we use 2 bytes for distance, max is 65535. Must be <= searchBufferSize_.
constexpr uint16_t MAX_DISTANCE = 4096;
// Max length we can encode (relative to lookahead buffer size and encoding scheme)
// If we use 1 byte for length, max is 255. Must be <= lookAheadBufferSize_.
constexpr uint8_t MAX_LENGTH = 255;

// --- Simple Hash for 3 bytes ---
// Using a basic rolling hash or just combining bytes might be sufficient.
// For simplicity, let's combine the first 3 bytes into a uint32_t.
// Note: More robust hashing could prevent collisions better.
inline uint32_t calculateHash(const std::vector<std::byte>& data, size_t pos) {
    if (pos + 2 >= data.size()) {
        return 0; // Indicate invalid hash if not enough bytes
    }
    // Simple combination - adjust bits as needed based on testing/profiling
    uint32_t h = static_cast<uint32_t>(data[pos]);
    h = (h << 8) | static_cast<uint32_t>(data[pos + 1]);
    h = (h << 8) | static_cast<uint32_t>(data[pos + 2]);
    return h;
    // Alternative: return std::hash<uint32_t>{}(combined_value);
}

// Constructor
Lz77Compressor::Lz77Compressor(size_t searchBufferSize, size_t lookAheadBufferSize) :
    searchBufferSize_(std::min(searchBufferSize, static_cast<size_t>(MAX_DISTANCE))), // Ensure config doesn't exceed encodable limits
    lookAheadBufferSize_(lookAheadBufferSize) // Lookahead doesn't directly limit encoding like distance
{
    if (searchBufferSize_ == 0 || lookAheadBufferSize_ == 0) {
        throw std::invalid_argument("LZ77 buffer sizes cannot be zero.");
    }
    if (lookAheadBufferSize_ < MIN_MATCH_LENGTH) {
       // Warning or adjustment might be needed if lookahead is too small for min match
       // For now, assume valid configuration.
    }
}

// Placeholder for findLongestMatch - implementation will follow
/* REMOVE THIS ENTIRE FUNCTION IMPLEMENTATION
Lz77Compressor::Match Lz77Compressor::findLongestMatch(const std::vector<std::byte>& data, size_t currentPosition, size_t searchBufferStart) const {
    // Trivial comment added to trigger CI
    Match bestMatch;
    bestMatch.length = 0; // No match found yet
    bestMatch.distance = 0;

    // ... (old implementation code) ...

    return bestMatch;
}
*/

std::vector<std::byte> Lz77Compressor::compress(const std::vector<std::byte>& data) const {
    if (data.empty()) {
        return {};
    }

    std::vector<std::byte> compressedData;
    compressedData.reserve(data.size() / 2); // Heuristic: aim for 50%

    // Hash table mapping: hash of 3 bytes -> most recent position
    std::unordered_map<uint32_t, size_t> hashTable;

    size_t currentPosition = 0;

    while (currentPosition < data.size()) {
        // --- Try to find a match ---
        Match bestMatch; // Reset best match for this position
        bestMatch.length = 0;
        bestMatch.distance = 0;

        // Only search if enough bytes remain for a minimum match
        if (currentPosition + MIN_MATCH_LENGTH <= data.size()) {
            uint32_t currentHash = calculateHash(data, currentPosition);
            auto it = hashTable.find(currentHash);

            if (it != hashTable.end()) {
                // Potential match found - verify it
                size_t potentialMatchPos = it->second;
                size_t currentSearchBufferStart = (currentPosition > searchBufferSize_) ? (currentPosition - searchBufferSize_) : 0;

                // Check if potential match is within the current search window
                if (potentialMatchPos >= currentSearchBufferStart) {
                    size_t currentMatchLength = 0;
                    const size_t maxPossibleLength = std::min(lookAheadBufferSize_, data.size() - currentPosition);

                    // Byte-by-byte comparison
                    while (currentMatchLength < maxPossibleLength &&
                           data[potentialMatchPos + currentMatchLength] == data[currentPosition + currentMatchLength])
                    {
                        currentMatchLength++;
                    }

                    // Check if this match is valid and better than any previous found *at this position*
                    // (Note: A simple hash table only stores the *most recent* pos. More complex structures
                    // like linked lists per hash entry could find older, potentially longer matches,
                    // but increase complexity. This is a common optimization trade-off).
                    if (currentMatchLength >= MIN_MATCH_LENGTH) {
                       // This is the longest match we can find using this simple hash lookup
                        bestMatch.length = std::min(currentMatchLength, static_cast<size_t>(MAX_LENGTH));
                        bestMatch.distance = currentPosition - potentialMatchPos;
                        // We already ensured distance <= searchBufferSize_ implicitly by checking potentialMatchPos >= currentSearchBufferStart
                    }
                }
            }
             // Update hash table *before* deciding encoding, but use the position that's *about to be processed*
             // This makes the *next* iteration potentially find the sequence we are processing now.
             if (currentHash != 0) { // Don't store invalid hash
                hashTable[currentHash] = currentPosition;
             }
        }
        // --- End Match Search ---


        // --- Encode based on whether a suitable match was found ---
        if (bestMatch.length >= MIN_MATCH_LENGTH) {
            // Encode as (Flag, Distance, Length)
            compressedData.push_back(PAIR_FLAG);

            uint16_t dist = static_cast<uint16_t>(bestMatch.distance);
            compressedData.push_back(static_cast<std::byte>(dist & 0xFF));
            compressedData.push_back(static_cast<std::byte>((dist >> 8) & 0xFF));

            uint8_t len = static_cast<uint8_t>(bestMatch.length);
            compressedData.push_back(static_cast<std::byte>(len));

            // --- IMPORTANT: Add skipped bytes to hash table ---
            // Since we jumped forward by match.length, we need to insert hashes
            // for the intermediate positions we skipped over so they can be found later.
            // Skip the last MIN_MATCH_LENGTH-1 bytes as they'll overlap with the next iteration's potential start.
            size_t limit = std::min(currentPosition + bestMatch.length - (MIN_MATCH_LENGTH -1), data.size());
             for (size_t i = currentPosition + 1; i < limit; ++i) {
                 if (i + MIN_MATCH_LENGTH <= data.size()) {
                    uint32_t skippedHash = calculateHash(data, i);
                     if (skippedHash != 0) {
                         hashTable[skippedHash] = i;
                     }
                 }
             }
             // --- End hash update for skipped bytes ---

            // Advance position by the length of the match
            currentPosition += bestMatch.length;

        } else {
            // Encode as (Flag, Literal)
            compressedData.push_back(LITERAL_FLAG);
            compressedData.push_back(data[currentPosition]);

            // Update hash table entry for the position we just encoded as literal
            // (This was already done above before the 'if (bestMatch.length ...)' block)

            // Advance position by 1
            currentPosition += 1;
        }
    }

    compressedData.shrink_to_fit();
    return compressedData;
}


// Placeholder for decompress - implementation will follow
std::vector<std::byte> Lz77Compressor::decompress(const std::vector<std::byte>& data) const {
     if (data.empty()) {
        return {};
    }
    std::vector<std::byte> decompressedData;
    decompressedData.reserve(data.size() * 3); // Heuristic - adjust as needed

    size_t currentPosition = 0;
    while(currentPosition < data.size()){
        std::byte flag = data[currentPosition++];

        if(flag == LITERAL_FLAG){
             if (currentPosition >= data.size()) {
                throw std::runtime_error("LZ77 Decompression Error: Truncated literal data.");
            }
            decompressedData.push_back(data[currentPosition++]);
        } else if (flag == PAIR_FLAG) {
             if (currentPosition + 2 >= data.size()) { // Need 3 bytes: dist_low, dist_high, len
                throw std::runtime_error("LZ77 Decompression Error: Truncated pair data.");
            }
            // Decode distance (2 bytes, Little-Endian)
            uint16_t distance = static_cast<uint16_t>(data[currentPosition]) |
                                (static_cast<uint16_t>(data[currentPosition+1]) << 8);
            currentPosition += 2;

            // Decode length (1 byte)
            // Add MIN_MATCH_LENGTH because lengths are typically stored relative (e.g., 0 means length 3)
            // *** We need to adjust the COMPRESSOR to store length-MIN_MATCH_LENGTH ***
            // ---> Let's adjust this later if needed, for now assume length is stored directly
             uint8_t length = static_cast<uint8_t>(data[currentPosition++]);


             if (distance == 0 || distance > decompressedData.size()) {
                throw std::runtime_error("LZ77 Decompression Error: Invalid distance.");
            }
             if (length < MIN_MATCH_LENGTH) {
                  // This can happen if MIN_MATCH_LENGTH > max encodable length, or malformed data
                  throw std::runtime_error("LZ77 Decompression Error: Invalid length decoded.");
            }

            size_t copyStartPosition = decompressedData.size() - distance;
             // Optimize copy for speed later (e.g., memcpy if non-overlapping, careful byte-by-byte if overlapping)
             decompressedData.reserve(decompressedData.size() + length); // Reserve space
            for (size_t i = 0; i < length; ++i) {
                decompressedData.push_back(decompressedData[copyStartPosition + i]);
            }

        } else {
             throw std::runtime_error("LZ77 Decompression Error: Invalid flag encountered.");
        }
    }

    // No need for shrink_to_fit here, reserve helps performance
    return decompressedData;
}


} // namespace compression 