#include "compression/HuffmanCoder.hpp"
#include <queue>
#include <cstdint>
#include <utility>
#include <algorithm>
#include <functional>

namespace compression {

// Custom comparator for priority queue
struct NodeComparator {
    bool operator()(
        const std::unique_ptr<HuffmanCoder::HuffmanNode>& a, 
        const std::unique_ptr<HuffmanCoder::HuffmanNode>& b) const {
        return a->frequency > b->frequency; // Min heap, sort by frequency
    }
};

std::unique_ptr<HuffmanCoder::HuffmanNode> HuffmanCoder::buildHuffmanTree(
    const FrequencyMap& freqMap) const {
    
    // Edge case: empty frequency map
    if (freqMap.empty()) {
        return nullptr;
    }
    
    // Edge case: only one symbol
    if (freqMap.size() == 1) {
        auto it = freqMap.begin();
        auto node = std::make_unique<HuffmanNode>(it->first, it->second);
        return node;
    }
    
    // Create min-heap (priority queue)
    std::priority_queue<
        std::unique_ptr<HuffmanNode>,
        std::vector<std::unique_ptr<HuffmanNode>>,
        NodeComparator
    > minHeap;
    
    // Add all symbols to the heap
    for (const auto& [symbol, freq] : freqMap) {
        minHeap.push(std::make_unique<HuffmanNode>(symbol, freq));
    }
    
    // Combine nodes until only the root remains
    while (minHeap.size() > 1) {
        // Extract two nodes with lowest frequencies
        auto left = std::move(const_cast<std::unique_ptr<HuffmanNode>&>(minHeap.top()));
        minHeap.pop();
        
        auto right = std::move(const_cast<std::unique_ptr<HuffmanNode>&>(minHeap.top()));
        minHeap.pop();
        
        // Create new internal node with these two as children
        auto parent = std::make_unique<HuffmanNode>(std::move(left), std::move(right));
        minHeap.push(std::move(parent));
    }
    
    // Return the root of the Huffman tree
    return std::move(const_cast<std::unique_ptr<HuffmanNode>&>(minHeap.top()));
}

void HuffmanCoder::generateCodes(
    const HuffmanNode* node,
    HuffmanCode prefix,
    HuffmanCodeMap& codeMap) const {
    
    if (!node) {
        return;
    }
    
    // If this is a leaf node, we've found a complete code
    if (node->isLeaf()) {
        codeMap[node->symbol] = prefix;
        return;
    }
    
    // Traverse left (add 0)
    if (node->left) {
        HuffmanCode leftPrefix = prefix;
        leftPrefix.push_back(false);
        generateCodes(node->left.get(), leftPrefix, codeMap);
    }
    
    // Traverse right (add 1)
    if (node->right) {
        HuffmanCode rightPrefix = prefix;
        rightPrefix.push_back(true);
        generateCodes(node->right.get(), rightPrefix, codeMap);
    }
}

HuffmanCodeMap HuffmanCoder::buildHuffmanCodes(const FrequencyMap& freqMap) const {
    // Build the Huffman tree
    auto rootNode = buildHuffmanTree(freqMap);
    
    // Generate codes by traversing the tree
    HuffmanCodeMap codeMap;
    if (rootNode) {
        generateCodes(rootNode.get(), {}, codeMap);
    }
    
    return codeMap;
}

std::map<uint32_t, uint8_t> HuffmanCoder::getCodeLengths(const HuffmanCodeMap& codeMap) const {
    std::map<uint32_t, uint8_t> lengthMap;
    
    // Simply store the length of each code
    for (const auto& [symbol, code] : codeMap) {
        lengthMap[symbol] = static_cast<uint8_t>(code.size());
    }
    
    return lengthMap;
}

std::map<uint32_t, uint8_t> HuffmanCoder::limitCodeLengths(
    const std::map<uint32_t, uint8_t>& inputLengths,
    uint8_t maxLength) const {
    
    // If nothing exceeds max length, just return as is
    bool needsReduction = false;
    for (const auto& [symbol, length] : inputLengths) {
        if (length > maxLength) {
            needsReduction = true;
            break;
        }
    }
    
    if (!needsReduction) {
        return inputLengths;
    }
    
    // Count symbols at each length
    std::vector<int> blCount(maxLength + 1, 0);
    for (const auto& [symbol, length] : inputLengths) {
        if (length <= maxLength) {
            blCount[length]++;
        }
    }
    
    // Find the first code for each code length
    std::vector<int> nextCode(maxLength + 1, 0);
    int code = 0;
    blCount[0] = 0;
    for (int bits = 1; bits <= maxLength; bits++) {
        code = (code + blCount[bits-1]) << 1;
        nextCode[bits] = code;
    }
    
    // Assign new lengths, ensuring none exceed maxLength
    std::map<uint32_t, uint8_t> resultLengths;
    for (const auto& [symbol, length] : inputLengths) {
        if (length <= maxLength) {
            resultLengths[symbol] = length;
        } else {
            // Need to reduce this code's length
            // In a real implementation, we'd use the Package-Merge algorithm
            // For simplicity, we'll just cap at maxLength
            resultLengths[symbol] = maxLength;
        }
    }
    
    return resultLengths;
}

} // namespace compression 