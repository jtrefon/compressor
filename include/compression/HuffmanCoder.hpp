#pragma once

#include <vector>
#include <map>
#include <queue>
#include <memory>
#include <cstdint>
#include <functional>
#include <limits>
#include <algorithm>

namespace compression {

// Forward declarations
using HuffmanCode = std::vector<bool>; // Represents a single Huffman code
using HuffmanCodeMap = std::map<uint32_t, HuffmanCode>; // Map symbol to code
using FrequencyMap = std::map<uint32_t, uint64_t>;

/**
 * @brief Class that handles Huffman coding functionality
 * 
 * This class implements the Strategy design pattern for Huffman coding operations.
 * It provides methods to build Huffman codes from frequency data and work with
 * Huffman trees.
 */
class HuffmanCoder {
public:
    /**
     * @brief Default constructor
     */
    HuffmanCoder() = default;

    /**
     * @brief Builds Huffman codes from a frequency map
     * 
     * @param freqMap Frequency map of symbols
     * @return HuffmanCodeMap Map of symbols to their Huffman codes
     */
    HuffmanCodeMap buildHuffmanCodes(const FrequencyMap& freqMap) const;

    /**
     * @brief Limits code lengths to a maximum value
     * 
     * @param inputLengths Map of symbols to their code lengths
     * @param maxLength Maximum allowed code length
     * @return std::map<uint32_t, uint8_t> Adjusted code lengths
     */
    std::map<uint32_t, uint8_t> limitCodeLengths(
        const std::map<uint32_t, uint8_t>& inputLengths,
        uint8_t maxLength) const;

    /**
     * @brief Gets code lengths from a Huffman code map
     * 
     * @param codeMap Map of symbols to their Huffman codes
     * @return std::map<uint32_t, uint8_t> Map of symbols to their code lengths
     */
    std::map<uint32_t, uint8_t> getCodeLengths(const HuffmanCodeMap& codeMap) const;

private:
    // Structure for Huffman tree nodes
    struct HuffmanNode {
        uint32_t symbol;
        uint64_t frequency;
        std::unique_ptr<HuffmanNode> left = nullptr;
        std::unique_ptr<HuffmanNode> right = nullptr;

        // Leaf node constructor
        HuffmanNode(uint32_t sym, uint64_t freq) 
            : symbol(sym), frequency(freq) {}

        // Internal node constructor
        HuffmanNode(std::unique_ptr<HuffmanNode> l, std::unique_ptr<HuffmanNode> r)
            : symbol(0), frequency(0), left(std::move(l)), right(std::move(r)) {
            if (left) frequency += left->frequency;
            if (right) frequency += right->frequency;
        }

        // Is this a leaf node?
        bool isLeaf() const {
            return left == nullptr && right == nullptr;
        }
    };

    /**
     * @brief Builds a Huffman tree from frequency data
     * 
     * @param freqMap Frequency map of symbols
     * @return std::unique_ptr<HuffmanNode> Root of the Huffman tree
     */
    std::unique_ptr<HuffmanNode> buildHuffmanTree(const FrequencyMap& freqMap) const;

    /**
     * @brief Generates Huffman codes by traversing the tree
     * 
     * @param node Current node in the tree
     * @param prefix Current code prefix (path in the tree)
     * @param codeMap Output map to store symbol-to-code mappings
     */
    void generateCodes(
        const HuffmanNode* node,
        HuffmanCode prefix,
        HuffmanCodeMap& codeMap) const;
};

} // namespace compression 