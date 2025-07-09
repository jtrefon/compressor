#include "compression/BwtCompressor.hpp"
#include "compression/HuffmanCompressor.hpp"
#include "compression/Lz77Compressor.hpp"
#include "compression/FileFormat.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <string>
#include <iostream>
#include <cstring>
#include <memory> // Required for std::make_unique

namespace compression {

//------------------------------------------------------------------------------
// MoveToFrontEncoder Implementation
//------------------------------------------------------------------------------

std::vector<uint8_t> MoveToFrontEncoder::encode(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    // Initialize the symbol table with values 0-255
    std::vector<uint8_t> symbolTable(256);
    std::iota(symbolTable.begin(), symbolTable.end(), 0);
    
    std::vector<uint8_t> result(data.size());
    
    // Encode each byte
    for (size_t i = 0; i < data.size(); ++i) {
        // Find the position of the symbol in the table
        const uint8_t symbol = data[i];
        auto it = std::find(symbolTable.begin(), symbolTable.end(), symbol);
        const size_t rank = std::distance(symbolTable.begin(), it);
        
        // Output the rank
        result[i] = static_cast<uint8_t>(rank);
        
        // Move the symbol to the front
        symbolTable.erase(it);
        symbolTable.insert(symbolTable.begin(), symbol);
    }
    
    return result;
}

std::vector<uint8_t> MoveToFrontEncoder::decode(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    // Initialize the symbol table with values 0-255
    std::vector<uint8_t> symbolTable(256);
    std::iota(symbolTable.begin(), symbolTable.end(), 0);
    
    std::vector<uint8_t> result(data.size());
    
    // Decode each byte
    for (size_t i = 0; i < data.size(); ++i) {
        // Get the rank and the corresponding symbol
        const uint8_t rank = data[i];
        const uint8_t symbol = symbolTable[rank];
        
        // Output the symbol
        result[i] = symbol;
        
        // Move the symbol to the front
        symbolTable.erase(symbolTable.begin() + rank);
        symbolTable.insert(symbolTable.begin(), symbol);
    }
    
    return result;
}

//------------------------------------------------------------------------------
// BwtCompressor Implementation
//------------------------------------------------------------------------------

// Improved suffix array construction using a more efficient method
// Uses the Suffix Array construction algorithm based on prefix doubling
struct SuffixArray {
    const std::vector<uint8_t>& data;
    std::vector<int32_t> SA; // Suffix Array
    
    explicit SuffixArray(const std::vector<uint8_t>& input) : data(input), SA(input.size()) {
        constructSuffixArray();
    }
    
    // Helper function to compare two rotations at indices i and j
    bool compareRotations(int32_t i, int32_t j) const {
        const size_t n = data.size();
        for (size_t k = 0; k < n; ++k) {
            uint8_t ci = data[(i + k) % n];
            uint8_t cj = data[(j + k) % n];
            
            if (ci != cj) {
                return ci < cj;
            }
        }
        return i < j; // For rotations that might be identical
    }
    
    void constructSuffixArray() {
        const size_t n = data.size();
        
        // For small inputs, use a simple approach for better performance
        if (n < 100) {
            // Initialize the suffix array with indices
            for (size_t i = 0; i < n; ++i) {
                SA[i] = static_cast<int32_t>(i);
            }
            
            // Sort the suffixes using a simple radix sorting approach for small arrays
            std::sort(SA.begin(), SA.end(), [this](int32_t i, int32_t j) {
                return compareRotations(i, j);
            });
            
            return;
        }
        
        // For larger inputs, use a more efficient algorithm
        // This is a simpler implementation of a prefix-doubling algorithm
        std::vector<int32_t> rank(n);
        std::vector<int32_t> newRank(n);
        std::vector<int32_t> count(std::max(n, static_cast<size_t>(256)), 0);
        
        // Initialize ranks with character values
        for (size_t i = 0; i < n; ++i) {
            rank[i] = data[i];
            SA[i] = static_cast<int32_t>(i);
        }
        
        // Iteratively refine the suffix array
        for (size_t h = 1; h < n; h *= 2) {
            // Sort by second part of pair
            std::fill(count.begin(), count.end(), 0);
            
            for (size_t i = 0; i < n; ++i) {
                int32_t pos = (SA[i] - static_cast<int32_t>(h) + static_cast<int32_t>(n)) % static_cast<int32_t>(n);
                ++count[rank[pos]];
            }
            
            for (size_t i = 1; i < count.size(); ++i) {
                count[i] += count[i - 1];
            }
            
            std::vector<int32_t> tempSA(n);
            for (int32_t i = static_cast<int32_t>(n) - 1; i >= 0; --i) {
                int32_t pos = (SA[i] - static_cast<int32_t>(h) + static_cast<int32_t>(n)) % static_cast<int32_t>(n);
                tempSA[--count[rank[pos]]] = pos;
            }
            std::swap(SA, tempSA);
            
            // Update ranks
            newRank[SA[0]] = 0;
            for (size_t i = 1; i < n; ++i) {
                bool different = rank[SA[i]] != rank[SA[i - 1]];
                if (!different && 
                    ((SA[i] + h < n && SA[i - 1] + h < n && rank[SA[i] + h] != rank[SA[i - 1] + h]) ||
                     (SA[i] + h >= n && SA[i - 1] + h < n) ||
                     (SA[i] + h < n && SA[i - 1] + h >= n))) {
                    different = true;
                }
                newRank[SA[i]] = newRank[SA[i - 1]] + (different ? 1 : 0);
            }
            std::swap(rank, newRank);
            
            // If all suffixes are sorted (each has a unique rank), we're done
            if (rank[SA[n - 1]] == static_cast<int32_t>(n) - 1) {
                break;
            }
        }
    }
};

BwtCompressor::BwtCompressor()
    : blockSize_(8 * 1024 * 1024), // 8MB block size by default
      mtfCoder_(),
      entropyCompressor_(std::make_unique<HuffmanCompressor>()) {
}

BwtCompressor::BwtCompressor(std::unique_ptr<ICompressor> entropyCompressor)
    : blockSize_(8 * 1024 * 1024),
      mtfCoder_(),
              entropyCompressor_(std::move(entropyCompressor)) {
    if (!entropyCompressor_) {
        entropyCompressor_ = std::make_unique<HuffmanCompressor>();
    }
}

std::pair<std::vector<uint8_t>, uint32_t> BwtCompressor::bwtEncode(const std::vector<uint8_t>& block) const {
    if (block.empty()) {
        return {{}, 0};
    }
    
    // Construct the suffix array
    SuffixArray sa(block);
    
    // Compute the BWT from the suffix array
    std::vector<uint8_t> bwt(block.size());
    uint32_t primaryIndex = 0;
    
    for (size_t i = 0; i < block.size(); ++i) {
        // The last character of the rotation starting at SA[i]
        size_t j = (sa.SA[i] + block.size() - 1) % block.size();
        bwt[i] = block[j];
        
        // Track the primary index (position of the original string)
        if (sa.SA[i] == 0) {
            primaryIndex = static_cast<uint32_t>(i);
        }
    }
    
    return {bwt, primaryIndex};
}

std::vector<uint8_t> BwtCompressor::bwtDecode(const std::vector<uint8_t>& block, uint32_t primaryIndex) const {
    const size_t n = block.size();
    if (n == 0) {
        return {};
    }
    
    // Check for invalid primary index
    if (primaryIndex >= n) {
        throw std::runtime_error("Invalid primary index in BWT decode");
    }

    // --- Core decoding logic ---
    // 1. Create a table of pairs (char, original_index)
    std::vector<std::pair<uint8_t, size_t>> table(n);
    for (size_t i = 0; i < n; ++i) {
        table[i] = {block[i], i};
    }
    
    // 2. Sort the table to get the first column of the BWT matrix
    std::stable_sort(table.begin(), table.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    
    // 3. Reconstruct the original string by walking backwards
    std::vector<uint8_t> decoded(n);
    size_t currentIndex = primaryIndex;
    for (size_t i = 0; i < n; ++i) {
        decoded[n - 1 - i] = table[currentIndex].first;
        currentIndex = table[currentIndex].second;
    }
    
    return decoded;
}

std::vector<uint8_t> BwtCompressor::runLengthEncode(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    std::vector<uint8_t> encoded;
    encoded.reserve(data.size());
    
    for (size_t i = 0; i < data.size(); ) {
        uint8_t currentByte = data[i];
        size_t count = 1;
        while (i + count < data.size() && data[i + count] == currentByte && count < 255) {
            count++;
        }
        
        encoded.push_back(currentByte);
        if (count > 1) {
            encoded.push_back(currentByte);
            encoded.push_back(static_cast<uint8_t>(count));
        }
        
        i += count;
    }
    
    return encoded;
}

std::vector<uint8_t> BwtCompressor::runLengthDecode(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    std::vector<uint8_t> decoded;
    decoded.reserve(data.size() * 2); // Pre-allocate to avoid reallocations
    
    for (size_t i = 0; i < data.size(); ) {
        uint8_t currentByte = data[i];
        decoded.push_back(currentByte);
        
        if (i + 2 < data.size() && data[i+1] == currentByte) {
            size_t count = data[i+2];
            for (size_t j = 1; j < count; ++j) {
                decoded.push_back(currentByte);
            }
            i += 3;
        } else {
            i++;
        }
    }
    
    return decoded;
}

std::vector<uint8_t> BwtCompressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {}; // Return empty vector for empty input
    }
    
    // Prepare result vector with header bytes for format version and flags
    std::vector<uint8_t> result;
    result.reserve(data.size()); // Reserve space for worst case
    
    // Header: [B][W][T][version][flags]
    // Where version = 1,
    //       flags bit 0 = data was transformed with MTF/RLE/Huffman
    result.push_back('B');
    result.push_back('W');
    result.push_back('T');
    result.push_back(1); // Version 1
    uint8_t flags = 0;
    bool useTransforms = data.size() >= 10;
    bool applyLz77 = false;
    if (useTransforms) {
        flags |= format::BWT_FLAG_TRANSFORMED;
        // Temporarily disable additional LZ77 stage due to stability issues
        applyLz77 = false;
    }
    result.push_back(flags);
    
    // For very small inputs, process as a single block without further compression
    if (data.size() < 10) {
        // Apply BWT
        auto [bwtBlock, primaryIndex] = bwtEncode(data);
        
        // Write block size and primary index directly
        uint32_t blockSize = static_cast<uint32_t>(bwtBlock.size());
        result.push_back(static_cast<uint8_t>((blockSize >> 24) & 0xFF));
        result.push_back(static_cast<uint8_t>((blockSize >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((blockSize >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(blockSize & 0xFF));
        
        result.push_back(static_cast<uint8_t>((primaryIndex >> 24) & 0xFF));
        result.push_back(static_cast<uint8_t>((primaryIndex >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((primaryIndex >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(primaryIndex & 0xFF));
        
        // Add the BWT-encoded block directly
        result.insert(result.end(), bwtBlock.begin(), bwtBlock.end());
        
        return result;
    }
    
    // Process data in blocks for larger inputs
    size_t actualBlockSize = ::std::min(blockSize_, data.size());

    // For larger inputs where blocking may cause issues, process as a single block
    // This ensures better compression and proper reconstruction
    if (data.size() <= 100000) { // 100KB threshold
        auto [bwtBlock, primaryIndex] = bwtEncode(data);
        auto mtfBlock = mtfCoder_.encode(bwtBlock);
        auto rleBlock = runLengthEncode(mtfBlock);
        auto compressedBlock = entropyCompressor_->compress(rleBlock);

        std::vector<uint8_t> finalBlock;
        if (applyLz77) {
            Lz77Compressor lz77;
            finalBlock = lz77.compress(compressedBlock);
        } else {
            finalBlock = ::std::move(compressedBlock);
        }

        // Write block size and primary index to result
        uint32_t blockSize = static_cast<uint32_t>(finalBlock.size());
        result.push_back(static_cast<uint8_t>((blockSize >> 24) & 0xFF));
        result.push_back(static_cast<uint8_t>((blockSize >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((blockSize >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(blockSize & 0xFF));
        
        result.push_back(static_cast<uint8_t>((primaryIndex >> 24) & 0xFF));
        result.push_back(static_cast<uint8_t>((primaryIndex >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((primaryIndex >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(primaryIndex & 0xFF));
        
        // Add the compressed block to the result
        result.insert(result.end(), finalBlock.begin(), finalBlock.end());
        
        return result;
    }
    
    // Process data in blocks for larger inputs
    for (size_t blockStart = 0; blockStart < data.size(); blockStart += actualBlockSize) {
        // Extract the current block
        size_t blockEnd = ::std::min(blockStart + actualBlockSize, data.size());
        std::vector<uint8_t> block(data.begin() + blockStart, data.begin() + blockEnd);
        
        // Apply Burrows-Wheeler Transform
        auto [bwtBlock, primaryIndex] = bwtEncode(block);
        
        // Apply Move-To-Front transform
        auto mtfBlock = mtfCoder_.encode(bwtBlock);
        
        // Apply Run-Length Encoding
        auto rleBlock = runLengthEncode(mtfBlock);

        // Apply entropy coding (Huffman)
        auto compressedBlock = entropyCompressor_->compress(rleBlock);

        std::vector<uint8_t> finalBlock;
        if (applyLz77) {
            Lz77Compressor lz77;
            finalBlock = lz77.compress(compressedBlock);
        } else {
            finalBlock = ::std::move(compressedBlock);
        }

        // Write block size and primary index to result
        uint32_t blockSize = static_cast<uint32_t>(finalBlock.size());
        result.push_back(static_cast<uint8_t>((blockSize >> 24) & 0xFF));
        result.push_back(static_cast<uint8_t>((blockSize >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((blockSize >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(blockSize & 0xFF));
        
        result.push_back(static_cast<uint8_t>((primaryIndex >> 24) & 0xFF));
        result.push_back(static_cast<uint8_t>((primaryIndex >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((primaryIndex >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(primaryIndex & 0xFF));
        
        // Add the compressed block to the result
        result.insert(result.end(), finalBlock.begin(), finalBlock.end());
    }
    
    return result;
}

std::vector<uint8_t> BwtCompressor::decompress(const std::vector<uint8_t>& data) const {
    // Handle empty input case consistently with compress
    if (data.empty()) {
        return {};
    }
    
    // Check for minimal header size
    if (data.size() < 5) {
        throw std::runtime_error("Invalid BWT compressed data: too small");
    }
    
    // Verify the header
    if (data[0] != 'B' || data[1] != 'W' || data[2] != 'T') {
        throw std::runtime_error("Not BWT compressed data: invalid signature");
    }
    
    uint8_t version = data[3];
    uint8_t flags = data[4];
    
    if (version != 1) {
        throw std::runtime_error("Unsupported BWT version: " + ::std::to_string(version));
    }
    
    bool transformed = (flags & format::BWT_FLAG_TRANSFORMED) != 0;
    bool lz77Applied = (flags & format::BWT_FLAG_LZ77) != 0;
    
    std::vector<uint8_t> result;
    size_t pos = 5; // Start after header
    
    // Process each block
    while (pos + 8 <= data.size()) {
        // Read block size and primary index
        uint32_t blockSize = 
            (static_cast<uint32_t>(data[pos]) << 24) |
            (static_cast<uint32_t>(data[pos + 1]) << 16) |
            (static_cast<uint32_t>(data[pos + 2]) << 8) |
            static_cast<uint32_t>(data[pos + 3]);
        pos += 4;
        
        uint32_t primaryIndex = 
            (static_cast<uint32_t>(data[pos]) << 24) |
            (static_cast<uint32_t>(data[pos + 1]) << 16) |
            (static_cast<uint32_t>(data[pos + 2]) << 8) |
            static_cast<uint32_t>(data[pos + 3]);
        pos += 4;
        
        // Check if block size is valid
        if (pos + blockSize > data.size()) {
            throw std::runtime_error("Invalid block size in BWT data: exceeds data bounds");
        }
        
        // Extract the compressed block
        std::vector<uint8_t> compressedBlock(data.begin() + pos, data.begin() + pos + blockSize);
        pos += blockSize;
        
        if (!transformed) {
            // Block stored as plain BWT output without further compression
            auto decodedBlock = bwtDecode(compressedBlock, primaryIndex);
            result.insert(result.end(), decodedBlock.begin(), decodedBlock.end());
            continue;
        }

        // If an extra LZ77 step was used, decode it first
        std::vector<uint8_t> afterLz;
        if (lz77Applied) {
            Lz77Compressor lz77;
            afterLz = lz77.decompress(compressedBlock);
        } else {
            afterLz = compressedBlock;
        }

        // Apply entropy decoding (Huffman)
        auto entropyDecodedBlock = entropyCompressor_->decompress(afterLz);

        // Apply Run-Length Decoding
        auto rleDecodedBlock = runLengthDecode(entropyDecodedBlock);

        // Apply Move-To-Front decoding
        auto mtfDecodedBlock = mtfCoder_.decode(rleDecodedBlock);

        // Apply inverse Burrows-Wheeler Transform
        auto bwtDecodedBlock = bwtDecode(mtfDecodedBlock, primaryIndex);
        
        // Add the decoded block to the result
        result.insert(result.end(), bwtDecodedBlock.begin(), bwtDecodedBlock.end());
    }
    
    return result;
}

} // namespace compression

