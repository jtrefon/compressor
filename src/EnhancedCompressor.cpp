#include <compression/EnhancedCompressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <algorithm>
#include <stdexcept>
#include <cmath>

namespace compression {

EnhancedCompressor::EnhancedCompressor() 
    : bwt_(std::make_unique<BwtCompressor>()),
      lz77_(std::make_unique<Lz77Compressor>(65536, 3, 258, false, true, true)),
      huffman_(std::make_unique<HuffmanCompressor>()) {
}

EnhancedCompressor::~EnhancedCompressor() = default;

std::vector<uint8_t> EnhancedCompressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    // Multi-stage compression for maximum ratio
    std::vector<uint8_t> compressed = data;
    
    // Stage 1: BWT transform for excellent compression on repetitive data
    // This is especially effective for text and structured data
    if (shouldUseBWT(data)) {
        compressed = bwt_->compress(compressed);
    }
    
    // Stage 2: LZ77 for dictionary-based compression
    // Works well on the BWT output and handles remaining patterns
    compressed = lz77_->compress(compressed);
    
    // Stage 3: Final Huffman coding for entropy compression
    // Squeezes out the remaining statistical redundancy
    compressed = huffman_->compress(compressed);
    
    return compressed;
}

std::vector<uint8_t> EnhancedCompressor::decompress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    // Reverse the compression stages
    std::vector<uint8_t> decompressed = data;
    
    try {
        // Stage 1: Huffman decompression
        decompressed = huffman_->decompress(decompressed);
        
        // Stage 2: LZ77 decompression  
        decompressed = lz77_->decompress(decompressed);
        
        // Stage 3: BWT inverse transform (if we detect BWT was used)
        if (detectBWTUsage(decompressed)) {
            decompressed = bwt_->decompress(decompressed);
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Enhanced decompression failed: ") + e.what());
    }
    
    return decompressed;
}

bool EnhancedCompressor::shouldUseBWT(const std::vector<uint8_t>& data) const {
    // BWT is most effective on data with high redundancy
    // Use simple heuristics to decide
    
    if (data.size() < 100) {
        return false; // Too small for BWT to be effective
    }
    
    // Calculate entropy - lower entropy means more redundancy
    std::array<size_t, 256> freq{};
    for (uint8_t byte : data) {
        freq[byte]++;
    }
    
    double entropy = 0.0;
    for (size_t count : freq) {
        if (count > 0) {
            double p = static_cast<double>(count) / data.size();
            entropy -= p * std::log2(p);
        }
    }
    
    // Use BWT if entropy is below threshold (indicating redundancy)
    return entropy < 7.0; // Max entropy is 8.0 for random data
}

bool EnhancedCompressor::detectBWTUsage(const std::vector<uint8_t>& data) const {
    // Simple heuristic: BWT output typically has long runs of similar characters
    // Check if there are unusually long runs
    
    if (data.size() < 50) {
        return false;
    }
    
    size_t maxRun = 1;
    size_t currentRun = 1;
    
    for (size_t i = 1; i < data.size(); ++i) {
        if (data[i] == data[i-1]) {
            currentRun++;
        } else {
            maxRun = std::max(maxRun, currentRun);
            currentRun = 1;
        }
    }
    maxRun = std::max(maxRun, currentRun);
    
    // If we have runs longer than expected for random data, likely BWT
    return maxRun > data.size() / 20; // More than 5% of data in one run
}

} // namespace compression
