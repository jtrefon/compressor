#pragma once

#include "ICompressor.hpp"
#include <vector>
#include <map>
#include <queue>
#include <memory>
#include <cstdint>
#include <functional>

namespace compression {

/**
 * @brief Implements ICompressor using Huffman coding.
 *
 * Uses canonical Huffman codes for potentially better efficiency.
 * Requires transmitting frequency table or tree structure.
 */
class HuffmanCompressor final : public ICompressor {
public:
    // Type aliases for clarity
    using HuffmanCode = std::vector<bool>; // Sequence of bits (0s and 1s)
    using HuffmanCodeMap = std::map<uint8_t, HuffmanCode>;
    using FrequencyMap = std::map<uint8_t, uint64_t>;

    // Structure for Huffman tree nodes
    struct HuffmanNode {
        uint8_t data;          
        uint64_t frequency;      
        std::unique_ptr<HuffmanNode> left = nullptr; // Ensure unique_ptr for ownership
        std::unique_ptr<HuffmanNode> right = nullptr;

        // Constructors using unique_ptr for children
        HuffmanNode(uint8_t d, uint64_t freq) : data(d), frequency(freq) {}
        HuffmanNode(std::unique_ptr<HuffmanNode> l, std::unique_ptr<HuffmanNode> r)
            : data{}, frequency(0), left(std::move(l)), right(std::move(r)) {
            if (left) frequency += left->frequency;
            if (right) frequency += right->frequency;
        }
    };

    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;

private:
    // --- Helper Methods (declarations) --- 
    FrequencyMap buildFrequencyMap(const std::vector<uint8_t>& data) const;
    // Returns unique_ptr to root.
    std::unique_ptr<HuffmanNode> buildHuffmanTree(const FrequencyMap& freqMap) const;
    void generateCodes(const HuffmanNode* node, HuffmanCode prefix, HuffmanCodeMap& codeMap) const;
    std::vector<uint8_t> serializeFrequencyMap(const FrequencyMap& freqMap) const;
    FrequencyMap deserializeFrequencyMap(const std::vector<uint8_t>& buffer, size_t& offset) const;
};

} // namespace compression 