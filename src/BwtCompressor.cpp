#include "compression/BwtCompressor.hpp"
#include "compression/HuffmanCompressor.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <string>
#include <iostream>
#include <cstring>

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
    : blockSize_(1024 * 1024), // 1MB block size by default
      mtfCoder_(),
      entropyCompressor_(std::make_unique<HuffmanCompressor>()) {
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
    if (block.empty()) {
        return {};
    }
    
    const size_t n = block.size();
    if (primaryIndex >= n) {
        throw std::runtime_error("Invalid primary index for BWT decoding");
    }
    
    // Count occurrences of each character
    std::vector<int32_t> count(256, 0);
    for (uint8_t c : block) {
        ++count[c];
    }
    
    // Compute the starting position for each character
    std::vector<int32_t> startPos(256, 0);
    for (int i = 1; i < 256; ++i) {
        startPos[i] = startPos[i-1] + count[i-1];
    }
    
    // Compute the transform array
    std::vector<int32_t> transform(n);
    std::vector<int32_t> tempPos = startPos; // Copy to avoid modifying startPos
    
    for (size_t i = 0; i < n; ++i) {
        uint8_t c = block[i];
        transform[tempPos[c]++] = static_cast<int32_t>(i);
    }
    
    // Reconstruct the original string
    std::vector<uint8_t> result(n);
    int32_t nextIndex = transform[primaryIndex];
    
    for (size_t i = 0; i < n; ++i) {
        result[i] = block[nextIndex];
        nextIndex = transform[nextIndex];
    }
    
    return result;
}

std::vector<uint8_t> BwtCompressor::runLengthEncode(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    std::vector<uint8_t> result;
    result.reserve(data.size()); // Reserve space for worst case
    
    // Simple RLE: for runs of 4 or more identical bytes
    // Use format: [0] [byte] [run length - 4]
    uint8_t currentByte = data[0];
    uint32_t runLength = 1;
    
    for (size_t i = 1; i < data.size(); ++i) {
        if (data[i] == currentByte) {
            ++runLength;
            
            // If we have a long run and reached the max run length or end of data
            if (runLength >= 4 && (runLength == 259 || i == data.size() - 1)) {
                // Encode the run
                result.push_back(0); // RLE marker
                result.push_back(currentByte);
                result.push_back(static_cast<uint8_t>(runLength - 4));
                
                // Reset the run
                runLength = 0;
                
                // If we're not at the end, prepare for the next byte
                if (i < data.size() - 1) {
                    currentByte = data[i + 1];
                    i++; // Skip the next byte as we've already processed it
                }
            }
        } else {
            // If we had a run of 4 or more, encode it
            if (runLength >= 4) {
                result.push_back(0); // RLE marker
                result.push_back(currentByte);
                result.push_back(static_cast<uint8_t>(runLength - 4));
            } else {
                // Otherwise, just output the bytes
                for (uint32_t j = 0; j < runLength; ++j) {
                    result.push_back(currentByte);
                }
            }
            
            // Start a new run
            currentByte = data[i];
            runLength = 1;
        }
    }
    
    // Handle any remaining bytes
    if (runLength > 0 && runLength < 4) {
        for (uint32_t j = 0; j < runLength; ++j) {
            result.push_back(currentByte);
        }
    }
    
    return result;
}

std::vector<uint8_t> BwtCompressor::runLengthDecode(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    std::vector<uint8_t> result;
    result.reserve(data.size() * 2); // Reserve space for potential expansion
    
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == 0 && i + 2 < data.size()) {
            // RLE block: [0] [byte] [run length - 4]
            uint8_t byte = data[i + 1];
            uint32_t runLength = data[i + 2] + 4; // Add 4 to get actual run length
            
            for (uint32_t j = 0; j < runLength; ++j) {
                result.push_back(byte);
            }
            
            i += 2; // Skip the byte and run length values
        } else {
            // Regular byte
            result.push_back(data[i]);
        }
    }
    
    return result;
}

std::vector<uint8_t> BwtCompressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {}; // Return empty vector for empty input
    }
    
    // Prepare result vector with header bytes for format version and flags
    std::vector<uint8_t> result;
    result.reserve(data.size()); // Reserve space for worst case
    
    // Header: [B][W][T][version][flags]
    // Where version = 1, flags bit 0 = RLE enabled, bit 1-7 reserved
    result.push_back('B');
    result.push_back('W');
    result.push_back('T');
    result.push_back(1); // Version 1
    result.push_back(1); // Flags: RLE enabled
    
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
    size_t actualBlockSize = std::min(blockSize_, data.size());

    // For larger inputs where blocking may cause issues, process as a single block
    // This ensures better compression and proper reconstruction
    if (data.size() <= 100000) { // 100KB threshold
        auto [bwtBlock, primaryIndex] = bwtEncode(data);
        auto mtfBlock = mtfCoder_.encode(bwtBlock);
        auto rleBlock = runLengthEncode(mtfBlock);
        auto compressedBlock = entropyCompressor_->compress(rleBlock);
        
        // Write block size and primary index to result
        uint32_t blockSize = static_cast<uint32_t>(compressedBlock.size());
        result.push_back(static_cast<uint8_t>((blockSize >> 24) & 0xFF));
        result.push_back(static_cast<uint8_t>((blockSize >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((blockSize >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(blockSize & 0xFF));
        
        result.push_back(static_cast<uint8_t>((primaryIndex >> 24) & 0xFF));
        result.push_back(static_cast<uint8_t>((primaryIndex >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((primaryIndex >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(primaryIndex & 0xFF));
        
        // Add the compressed block to the result
        result.insert(result.end(), compressedBlock.begin(), compressedBlock.end());
        
        return result;
    }
    
    // Process data in blocks for larger inputs
    for (size_t blockStart = 0; blockStart < data.size(); blockStart += actualBlockSize) {
        // Extract the current block
        size_t blockEnd = std::min(blockStart + actualBlockSize, data.size());
        std::vector<uint8_t> block(data.begin() + blockStart, data.begin() + blockEnd);
        
        // Apply Burrows-Wheeler Transform
        auto [bwtBlock, primaryIndex] = bwtEncode(block);
        
        // Apply Move-To-Front transform
        auto mtfBlock = mtfCoder_.encode(bwtBlock);
        
        // Apply Run-Length Encoding
        auto rleBlock = runLengthEncode(mtfBlock);
        
        // Apply entropy coding (Huffman)
        auto compressedBlock = entropyCompressor_->compress(rleBlock);
        
        // Write block size and primary index to result
        uint32_t blockSize = static_cast<uint32_t>(compressedBlock.size());
        result.push_back(static_cast<uint8_t>((blockSize >> 24) & 0xFF));
        result.push_back(static_cast<uint8_t>((blockSize >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((blockSize >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(blockSize & 0xFF));
        
        result.push_back(static_cast<uint8_t>((primaryIndex >> 24) & 0xFF));
        result.push_back(static_cast<uint8_t>((primaryIndex >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((primaryIndex >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(primaryIndex & 0xFF));
        
        // Add the compressed block to the result
        result.insert(result.end(), compressedBlock.begin(), compressedBlock.end());
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
        throw std::runtime_error("Unsupported BWT version: " + std::to_string(version));
    }
    
    bool rleEnabled = (flags & 1) != 0;
    
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
        
        // For very small blocks, they might be stored directly without additional compression
        if (blockSize <= 10 && compressedBlock.size() == blockSize) {
            // Apply inverse BWT directly
            auto decodedBlock = bwtDecode(compressedBlock, primaryIndex);
            result.insert(result.end(), decodedBlock.begin(), decodedBlock.end());
            continue;
        }
        
        // Apply entropy decoding (Huffman)
        auto entropyDecodedBlock = entropyCompressor_->decompress(compressedBlock);
        
        // Apply Run-Length Decoding if enabled
        auto rleDecodedBlock = rleEnabled ? runLengthDecode(entropyDecodedBlock) : entropyDecodedBlock;
        
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
