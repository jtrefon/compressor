#include "compression/RleCompressor.hpp"
#include <stdexcept>

namespace compression {

// Simple RLE format: [Count][Value][Count][Value]...
// Count is stored as a single byte (unsigned char).
// Max run length is 255.

std::vector<uint8_t> RleCompressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    std::vector<uint8_t> compressed;
    
    // Simple RLE implementation
    uint8_t current = data[0];
    uint8_t count = 1;
    
    for (size_t i = 1; i < data.size(); ++i) {
        if (data[i] == current && count < 255) {
            // Continue the current run
            count++;
        } else {
            // Output the current run
            compressed.push_back(count);
            compressed.push_back(current);
            
            // Start a new run
            current = data[i];
            count = 1;
        }
    }
    
    // Output the final run
    compressed.push_back(count);
    compressed.push_back(current);
    
    return compressed;
}

std::vector<uint8_t> RleCompressor::decompress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    if (data.size() % 2 != 0) {
        throw std::runtime_error("Invalid RLE data: length must be even");
    }
    
    std::vector<uint8_t> decompressed;
    
    // Process each (count, value) pair
    for (size_t i = 0; i < data.size(); i += 2) {
        uint8_t count = data[i];
        uint8_t value = data[i + 1];
        
        // Expand the run
        for (uint8_t j = 0; j < count; ++j) {
            decompressed.push_back(value);
        }
    }
    
    return decompressed;
}

} // namespace compression 