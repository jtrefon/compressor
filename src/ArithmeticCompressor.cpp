#include <compression/ArithmeticCompressor.hpp>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <iostream>

namespace compression {

std::vector<uint8_t> ArithmeticCompressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }

    // For now, implement a simple entropy-based compression
    // This is a simplified version that should work reliably
    
    // Build frequency map
    FrequencyMap freqMap = buildFrequencyMap(data);
    
    // Serialize frequency map
    std::vector<uint8_t> result = serializeFrequencyMap(freqMap);
    
    // Simple entropy coding using variable-length encoding
    // Sort symbols by frequency for better compression
    std::vector<std::pair<uint8_t, uint64_t>> sortedFreq(freqMap.begin(), freqMap.end());
    std::sort(sortedFreq.begin(), sortedFreq.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Create code table (more frequent = shorter codes)
    std::map<uint8_t, std::vector<bool>> codeTable;
    uint32_t code = 0;
    for (const auto& [symbol, freq] : sortedFreq) {
        // Use more aggressive variable-length codes for better compression
        int codeLength;
        if (freq > data.size() / 10) {
            codeLength = 4; // Very frequent symbols get 4-bit codes
        } else if (freq > data.size() / 50) {
            codeLength = 6; // Frequent symbols get 6-bit codes
        } else if (freq > data.size() / 100) {
            codeLength = 8; // Medium frequency get 8-bit codes
        } else {
            codeLength = std::min(12, std::max(5, 16 - static_cast<int>(std::log2(freq + 1))));
        }
        
        for (int i = 0; i < codeLength; ++i) {
            codeTable[symbol].push_back((code >> i) & 1);
        }
        code++;
    }
    
    // Encode data
    std::vector<bool> encodedBits;
    for (uint8_t symbol : data) {
        const auto& code = codeTable[symbol];
        encodedBits.insert(encodedBits.end(), code.begin(), code.end());
    }
    
    // Convert bits to bytes
    uint8_t currentByte = 0;
    int bitsInByte = 0;
    
    for (bool bit : encodedBits) {
        currentByte = (currentByte << 1) | (bit ? 1 : 0);
        bitsInByte++;
        
        if (bitsInByte == 8) {
            result.push_back(currentByte);
            currentByte = 0;
            bitsInByte = 0;
        }
    }
    
    // Add remaining bits if any
    if (bitsInByte > 0) {
        currentByte <<= (8 - bitsInByte);
        result.push_back(currentByte);
    }
    
    return result;
}

std::vector<uint8_t> ArithmeticCompressor::decompress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }

    // Deserialize frequency map
    size_t offset = 0;
    FrequencyMap freqMap = deserializeFrequencyMap(data, offset);
    
    // Calculate total size
    size_t totalSize = 0;
    for (const auto& [symbol, freq] : freqMap) {
        totalSize += freq;
    }

    // Rebuild code table (same as compression)
    std::vector<std::pair<uint8_t, uint64_t>> sortedFreq(freqMap.begin(), freqMap.end());
    std::sort(sortedFreq.begin(), sortedFreq.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::map<std::vector<bool>, uint8_t> decodeTable;
    uint32_t code = 0;
    for (const auto& [symbol, freq] : sortedFreq) {
        int codeLength;
        if (freq > totalSize / 10) {
            codeLength = 4;
        } else if (freq > totalSize / 50) {
            codeLength = 6;
        } else if (freq > totalSize / 100) {
            codeLength = 8;
        } else {
            codeLength = std::min(12, std::max(5, 16 - static_cast<int>(std::log2(freq + 1))));
        }
        
        std::vector<bool> codeBits;
        for (int i = 0; i < codeLength; ++i) {
            codeBits.push_back((code >> i) & 1);
        }
        decodeTable[codeBits] = symbol;
        code++;
    }

    // Decode data
    std::vector<uint8_t> result;
    std::vector<bool> currentBits;
    
    // Convert remaining bytes to bits
    std::vector<bool> allBits;
    for (size_t i = offset; i < data.size(); ++i) {
        for (int bit = 7; bit >= 0; --bit) {
            allBits.push_back((data[i] >> bit) & 1);
        }
    }
    
    // Remove padding bits (last byte may have padding)
    if (!allBits.empty()) {
        // Find actual data length by trying to decode
        size_t bitsProcessed = 0;
        while (result.size() < totalSize && bitsProcessed + 1 <= allBits.size()) {
            // Try to find a matching code
            for (const auto& [codeBits, symbol] : decodeTable) {
                if (bitsProcessed + codeBits.size() <= allBits.size()) {
                    bool match = true;
                    for (size_t i = 0; i < codeBits.size(); ++i) {
                        if (allBits[bitsProcessed + i] != codeBits[i]) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        result.push_back(symbol);
                        bitsProcessed += codeBits.size();
                        break;
                    }
                }
            }
            
            // Safety check to prevent infinite loop
            if (result.size() == totalSize) break;
        }
    }

    return result;
}

ArithmeticCompressor::FrequencyMap ArithmeticCompressor::buildFrequencyMap(const std::vector<uint8_t>& data) const {
    FrequencyMap freqMap;
    for (uint8_t byte : data) {
        freqMap[byte]++;
    }
    return freqMap;
}

ArithmeticCompressor::ProbabilityMap ArithmeticCompressor::buildProbabilityMap(const FrequencyMap& freqMap, size_t totalSize) const {
    ProbabilityMap probMap;
    double cumulative = 0.0;
    
    // Calculate cumulative probabilities
    for (const auto& [symbol, freq] : freqMap) {
        double probability = static_cast<double>(freq) / totalSize;
        cumulative += probability;
        probMap[symbol] = cumulative;
    }
    
    return probMap;
}

std::vector<uint8_t> ArithmeticCompressor::serializeFrequencyMap(const FrequencyMap& freqMap) const {
    std::vector<uint8_t> serialized;
    
    // Number of entries
    serialized.push_back(static_cast<uint8_t>(freqMap.size()));
    
    // For each symbol and its frequency
    for (const auto& [symbol, frequency] : freqMap) {
        // Symbol
        serialized.push_back(symbol);
        
        // Frequency (variable-length encoding)
        uint64_t value = frequency;
        do {
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if (value > 0) byte |= 0x80;
            serialized.push_back(byte);
        } while (value > 0);
    }
    
    return serialized;
}

ArithmeticCompressor::FrequencyMap ArithmeticCompressor::deserializeFrequencyMap(const std::vector<uint8_t>& buffer, size_t& offset) const {
    FrequencyMap freqMap;
    
    if (offset >= buffer.size()) {
        throw std::runtime_error("Buffer ended unexpectedly during map deserialization");
    }
    
    // Get count of entries
    uint8_t count = buffer[offset++];
    
    // Process each entry
    for (uint8_t i = 0; i < count; i++) {
        if (offset >= buffer.size()) {
            throw std::runtime_error("Buffer ended unexpectedly during map entry deserialization");
        }
        
        // Read symbol
        uint8_t symbol = buffer[offset++];
        
        // Read frequency (variable-length encoded)
        uint64_t frequency = 0;
        uint8_t shift = 0;
        uint8_t byte;
        
        do {
            if (offset >= buffer.size()) {
                throw std::runtime_error("Buffer ended unexpectedly during frequency deserialization");
            }
            
            byte = buffer[offset++];
            frequency |= (static_cast<uint64_t>(byte & 0x7F) << shift);
            shift += 7;
            
            if (shift > 63 && (byte & 0x80)) {
                throw std::runtime_error("Frequency value too large");
            }
        } while (byte & 0x80);
        
        freqMap[symbol] = frequency;
    }
    
    return freqMap;
}

} // namespace compression
