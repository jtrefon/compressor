#pragma once

#include "compression/HuffmanCoder.hpp"
#include <memory>

namespace compression {

// Custom comparator for priority queue of HuffmanNodes
struct NodeComparator {
    bool operator()(
        const std::unique_ptr<HuffmanCoder::HuffmanNode>& a, 
        const std::unique_ptr<HuffmanCoder::HuffmanNode>& b) const {
        return a->frequency > b->frequency; // Min heap, sort by frequency
    }
};

} // namespace compression 