#pragma once

#include <vector>
#include <map>
#include <cstdint>
#include <limits>
#include <algorithm>

namespace compression {

// Forward declarations
using FrequencyMap = std::map<uint32_t, uint64_t>;

/**
 * @brief Class that handles Arithmetic coding functionality
 * 
 * This class implements the Strategy design pattern for Arithmetic coding operations.
 * It provides methods to encode and decode data using arithmetic coding, which offers
 * better compression ratios than Huffman coding for many types of data.
 */
class ArithmeticCoder {
public:
    // Types for arithmetic coding
    using Code = uint64_t;
    using Probability = double;
    
    // Constants for arithmetic coding
    static constexpr Code CODE_BITS = 64;
    static constexpr Code TOP_VALUE = (static_cast<Code>(1) << (CODE_BITS - 1)) - 1;
    static constexpr Code FIRST_QTR = TOP_VALUE / 4 + 1;
    static constexpr Code HALF = 2 * FIRST_QTR;
    static constexpr Code THIRD_QTR = 3 * FIRST_QTR;
    
    // Special symbol for end of data marker
    static constexpr uint32_t EOF_SYMBOL = std::numeric_limits<uint32_t>::max();

    /**
     * @brief Default constructor
     */
    ArithmeticCoder() = default;

    /**
     * @brief Builds probability model from frequency map
     * 
     * Converts a frequency map to cumulative probabilities needed for arithmetic coding
     * 
     * @param freqMap Frequency map of symbols
     * @return std::map<uint32_t, std::pair<uint64_t, uint64_t>> Map of symbols to cumulative frequency range
     */
    std::map<uint32_t, std::pair<uint64_t, uint64_t>> buildProbabilityModel(
        const FrequencyMap& freqMap) const;

    /**
     * @brief Encodes a sequence of symbols using arithmetic coding
     * 
     * @param symbols Sequence of symbols to encode
     * @param probModel Probability model for the symbols
     * @param totalFreq Total frequency count from the model
     * @return std::vector<uint8_t> Encoded data
     */
    std::vector<uint8_t> encode(
        const std::vector<uint32_t>& symbols,
        const std::map<uint32_t, std::pair<uint64_t, uint64_t>>& probModel,
        uint64_t totalFreq) const;
    
    /**
     * @brief Decodes a sequence of bytes using arithmetic coding
     * 
     * @param encodedData Encoded data bytes
     * @param probModel Probability model for the symbols
     * @param totalFreq Total frequency count from the model
     * @param numSymbols Number of symbols to decode
     * @return std::vector<uint32_t> Decoded symbols
     */
    std::vector<uint32_t> decode(
        const std::vector<uint8_t>& encodedData,
        const std::map<uint32_t, std::pair<uint64_t, uint64_t>>& probModel,
        uint64_t totalFreq,
        size_t numSymbols) const;
    
    /**
     * @brief Creates reverse mapping from cumulative frequency to symbol
     * 
     * @param probModel Probability model
     * @return std::map<uint64_t, uint32_t> Reverse mapping from cumulative frequency to symbol
     */
    std::map<uint64_t, uint32_t> createReverseMapping(
        const std::map<uint32_t, std::pair<uint64_t, uint64_t>>& probModel) const;
};

} // namespace compression 