#include <compression/Lz77Compressor.hpp>
#include <vector>
#include <stdexcept> // For potential errors
#include <algorithm> // For std::min
#include <cstring>   // For std::memcpy, memcmp (can optimize later)
#include <limits>    // For numeric_limits

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
constexpr uint8_t MAX_LENGTH = 32;


// Constructor
Lz77Compressor::Lz77Compressor(size_t searchBufferSize, size_t lookAheadBufferSize) :
    searchBufferSize_(std::min(searchBufferSize, static_cast<size_t>(MAX_DISTANCE))), // Ensure config doesn't exceed encodable limits
    lookAheadBufferSize_(std::min(lookAheadBufferSize, static_cast<size_t>(MAX_LENGTH)))
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
Lz77Compressor::Match Lz77Compressor::findLongestMatch(const std::vector<std::byte>& data, size_t currentPosition, size_t searchBufferStart) const {
    Match bestMatch;
    bestMatch.length = 0; // No match found yet
    bestMatch.distance = 0;

    // --- Implementation of search logic goes here ---
    // Iterate backwards through the search buffer (from currentPosition-1 down to searchBufferStart)
    // For each potential starting point in the search buffer:
    //   Compare bytes sequentially with the lookahead buffer (starting at currentPosition)
    //   Keep track of the longest match found so far (length and distance)
    //   Ensure match length doesn't exceed lookAheadBufferSize_
    //   Ensure distance doesn't exceed searchBufferSize_

    // Example outline:
    const size_t maxPossibleLength = std::min(lookAheadBufferSize_, data.size() - currentPosition);
    if (maxPossibleLength < MIN_MATCH_LENGTH) {
        return bestMatch; // Not enough data left for a minimum match
    }

    for (size_t searchPos = currentPosition - 1; searchPos >= searchBufferStart && searchPos < currentPosition /* prevent underflow */; --searchPos) {
        size_t currentMatchLength = 0;
        while (currentMatchLength < maxPossibleLength &&
               data[searchPos + currentMatchLength] == data[currentPosition + currentMatchLength])
        {
            currentMatchLength++;
        }

        if (currentMatchLength >= MIN_MATCH_LENGTH && currentMatchLength > bestMatch.length) {
            bestMatch.length = currentMatchLength;
            bestMatch.distance = currentPosition - searchPos; // Calculate distance

            // Optimization: If we found the maximum possible length, we can stop searching
            if (bestMatch.length == maxPossibleLength) {
                 break;
            }
        }
         // Handle edge case for loop condition when searchPos becomes 0
         if (searchPos == 0) break;
    }


    // Clamp length and distance to encodable limits (important!)
    bestMatch.length = std::min(bestMatch.length, static_cast<size_t>(MAX_LENGTH));
    bestMatch.distance = std::min(bestMatch.distance, static_cast<size_t>(MAX_DISTANCE));


    return bestMatch;
}


std::vector<std::byte> Lz77Compressor::compress(const std::vector<std::byte>& data) const {
    if (data.empty()) {
        return {};
    }

    std::vector<std::byte> compressedData;
    // Heuristic reservation: Compression factor varies wildly. Start with input size?
    compressedData.reserve(data.size());

    size_t currentPosition = 0;

    while (currentPosition < data.size()) {
        // Define the current search buffer bounds
        size_t searchBufferStart = (currentPosition > searchBufferSize_) ? (currentPosition - searchBufferSize_) : 0;

        // Find the longest match in the search buffer for the lookahead buffer
        Match match = findLongestMatch(data, currentPosition, searchBufferStart);

        // Encode based on whether a suitable match was found
        if (match.length >= MIN_MATCH_LENGTH) {
            // Encode as (Flag, Distance, Length)
            compressedData.push_back(PAIR_FLAG);

            // Encode distance (e.g., 2 bytes, little-endian or big-endian - choose one)
            // Let's use Little-Endian for this example
            uint16_t dist = static_cast<uint16_t>(match.distance);
            compressedData.push_back(static_cast<std::byte>(dist & 0xFF));          // Low byte
            compressedData.push_back(static_cast<std::byte>((dist >> 8) & 0xFF)); // High byte

            // Encode length (e.g., 1 byte)
            uint8_t len = static_cast<uint8_t>(match.length);
            compressedData.push_back(static_cast<std::byte>(len));

            // Advance position by the length of the match
            currentPosition += match.length;
        } else {
            // Encode as (Flag, Literal)
            compressedData.push_back(LITERAL_FLAG);
            compressedData.push_back(data[currentPosition]);

            // Advance position by 1 (just processed one literal)
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
    // Reserve estimate - can be much larger than compressed size
    decompressedData.reserve(data.size() * 2); // Initial guess

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
            uint8_t length = static_cast<uint8_t>(data[currentPosition++]);

             if (distance == 0 || distance > decompressedData.size()) {
                throw std::runtime_error("LZ77 Decompression Error: Invalid distance.");
            }
             if (length < MIN_MATCH_LENGTH) {
                  // This shouldn't happen if compressor works correctly and MIN_MATCH_LENGTH >= 3
                  throw std::runtime_error("LZ77 Decompression Error: Invalid length.");
            }

            size_t copyStartPosition = decompressedData.size() - distance;
            for (size_t i = 0; i < length; ++i) {
                // Ensure we don't read past the source during copy (can happen with overlapping matches)
                if (copyStartPosition + i >= decompressedData.size()) {
                   throw std::runtime_error("LZ77 Decompression Error: Copy read past end of available data (potential overlap issue).");
                }
                decompressedData.push_back(decompressedData[copyStartPosition + i]);
            }

        } else {
             throw std::runtime_error("LZ77 Decompression Error: Invalid flag encountered.");
        }

    }


    decompressedData.shrink_to_fit();
    return decompressedData;
}


} // namespace compression 