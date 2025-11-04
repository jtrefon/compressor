#pragma once

#include "ICompressor.hpp"
#include <vector>
#include <map>
#include <cstdint>
#include <memory>

namespace compression {

/**
 * @class ArithmeticCompressor
 * @brief Implements arithmetic coding for near-optimal entropy compression
 * 
 * Arithmetic coding can achieve compression ratios closer to the theoretical
 * entropy limit compared to Huffman coding, typically providing 5-15% better
 * compression on the same data.
 * 
 * This implementation uses adaptive probability modeling for better performance
 * on diverse data types.
 */
class ArithmeticCompressor : public ICompressor {
public:
    // Type aliases
    using FrequencyMap = std::map<uint8_t, uint64_t>;
    using ProbabilityMap = std::map<uint8_t, double>;

    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;

private:
    // Constants for arithmetic coding precision
    static constexpr uint32_t CODE_BITS = 32;
    static constexpr uint32_t HALF = 1u << 31;
    static constexpr uint32_t QUARTER1 = 1u << 30;
    static constexpr uint32_t QUARTER3 = 3u << 30;
    static constexpr uint32_t MAX_FREQ = 1u << 30;

    // Helper methods
    FrequencyMap buildFrequencyMap(const std::vector<uint8_t>& data) const;
    ProbabilityMap buildProbabilityMap(const FrequencyMap& freqMap, size_t totalSize) const;
    std::vector<uint8_t> serializeFrequencyMap(const FrequencyMap& freqMap) const;
    FrequencyMap deserializeFrequencyMap(const std::vector<uint8_t>& buffer, size_t& offset) const;
    
    // Core arithmetic coding methods
    void encodeSymbol(uint32_t& low, uint32_t& high, uint32_t& pending_bits,
                     uint8_t symbol, const ProbabilityMap& probMap,
                     std::vector<uint8_t>& output) const;
    uint8_t decodeSymbol(uint32_t& low, uint32_t& high, uint32_t value,
                        const ProbabilityMap& probMap,
                        const std::vector<uint8_t>& input, size_t& inputPos, size_t& bitPos) const;
    
    // Bit manipulation helpers
    void outputBit(uint32_t& pending_bits, bool bit, std::vector<uint8_t>& output) const;
    bool inputBit(const std::vector<uint8_t>& input, size_t& inputPos, size_t& bitPos) const;
    uint32_t inputValue(const std::vector<uint8_t>& input, size_t& inputPos, size_t& bitPos) const;
};

} // namespace compression
