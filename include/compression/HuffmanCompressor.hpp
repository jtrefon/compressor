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
    using HuffmanCodeMap = std::map<std::byte, HuffmanCode>;
    using FrequencyMap = std::map<std::byte, uint64_t>;

private:
    // Structure for Huffman tree nodes
    struct HuffmanNode {
        std::byte data;          
        uint64_t frequency;      
        std::unique_ptr<HuffmanNode> left = nullptr; // Ensure unique_ptr for ownership
        std::unique_ptr<HuffmanNode> right = nullptr;

        // Constructors using unique_ptr for children
        HuffmanNode(std::byte d, uint64_t freq) : data(d), frequency(freq) {}
        HuffmanNode(std::unique_ptr<HuffmanNode> l, std::unique_ptr<HuffmanNode> r)
            : data{}, frequency(0), left(std::move(l)), right(std::move(r)) {
            if (left) frequency += left->frequency;
            if (right) frequency += right->frequency;
        }
    };

    // --- Helper Methods (declarations) --- 
    FrequencyMap buildFrequencyMap(const std::vector<std::byte>& data) const;
    // Returns unique_ptr to root.
    std::unique_ptr<HuffmanNode> buildHuffmanTree(const FrequencyMap& freqMap) const;
    void generateCodes(const HuffmanNode* node, HuffmanCode prefix, HuffmanCodeMap& codeMap) const;
    std::vector<std::byte> serializeFrequencyMap(const FrequencyMap& freqMap) const;
    FrequencyMap deserializeFrequencyMap(const std::vector<std::byte>& buffer, size_t& offset) const;

public:
    std::vector<std::byte> compress(const std::vector<std::byte>& data) const override;
    std::vector<std::byte> decompress(const std::vector<std::byte>& data) const override;
};

} // namespace compression 