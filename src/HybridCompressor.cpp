#include <compression/HybridCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <algorithm>
#include <map>

namespace compression {

HybridCompressor::HybridCompressor() 
    : lz77_(std::make_unique<Lz77Compressor>(65536, 3, 258, false, true, true)),
      huffman_(std::make_unique<HuffmanCompressor>()) {
}

HybridCompressor::~HybridCompressor() = default;

std::vector<uint8_t> HybridCompressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    // Analyze data to choose best approach
    DataAnalysis analysis = analyzeData(data);
    
    if (analysis.hasGoodLZ77Potential) {
        return compressWithLZ77Plus(data);
    } else {
        return compressWithHuffmanPlus(data);
    }
}

std::vector<uint8_t> HybridCompressor::decompress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    try {
        // Check method marker
        if (data.size() < 1) {
            throw std::runtime_error("Invalid compressed data");
        }
        
        uint8_t method = data[0];
        const std::vector<uint8_t> compressedData(data.begin() + 1, data.end());
        
        switch (method) {
            case 0x01: // LZ77+ method
                return decompressWithLZ77Plus(compressedData);
            case 0x02: // Huffman+ method
                return decompressWithHuffmanPlus(compressedData);
            default:
                throw std::runtime_error("Unknown compression method");
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Hybrid decompression failed: ") + e.what());
    }
}

DataAnalysis HybridCompressor::analyzeData(const std::vector<uint8_t>& data) const {
    DataAnalysis analysis;
    
    if (data.size() < 100) {
        return analysis;
    }
    
    // Check for repeated patterns (good for LZ77)
    size_t patternMatches = 0;
    const size_t patternLength = 4;
    
    for (size_t i = 0; i + patternLength < data.size(); ++i) {
        std::vector<uint8_t> pattern(data.begin() + i, data.begin() + i + patternLength);
        
        // Look for this pattern later in the data
        for (size_t j = i + patternLength; j + patternLength <= data.size(); ++j) {
            if (std::equal(pattern.begin(), pattern.end(), data.begin() + j)) {
                patternMatches++;
                break;
            }
        }
    }
    
    analysis.hasGoodLZ77Potential = (patternMatches > data.size() * 0.01); // 1% threshold
    
    return analysis;
}

std::vector<uint8_t> HybridCompressor::compressWithLZ77Plus(const std::vector<uint8_t>& data) const {
    // Use LZ77 first, then apply secondary compression
    auto lz77Compressed = lz77_->compress(data);
    
    // Apply RLE to LZ77 output for additional compression
    auto rleCompressed = applyRLE(lz77Compressed);
    
    // Apply Huffman to the result
    auto finalCompressed = huffman_->compress(rleCompressed);
    
    // Add method marker
    std::vector<uint8_t> result;
    result.push_back(0x01);
    result.insert(result.end(), finalCompressed.begin(), finalCompressed.end());
    
    return result;
}

std::vector<uint8_t> HybridCompressor::compressWithHuffmanPlus(const std::vector<uint8_t>& data) const {
    // Apply preprocessing to improve Huffman efficiency
    auto preprocessed = preprocessForHuffman(data);
    
    // Apply Huffman compression
    auto huffmanCompressed = huffman_->compress(preprocessed);
    
    // Add method marker
    std::vector<uint8_t> result;
    result.push_back(0x02);
    result.insert(result.end(), huffmanCompressed.begin(), huffmanCompressed.end());
    
    return result;
}

std::vector<uint8_t> HybridCompressor::decompressWithLZ77Plus(const std::vector<uint8_t>& data) const {
    // Reverse the compression steps
    auto huffmanDecompressed = huffman_->decompress(data);
    auto rleDecompressed = removeRLE(huffmanDecompressed);
    auto lz77Decompressed = lz77_->decompress(rleDecompressed);
    
    return lz77Decompressed;
}

std::vector<uint8_t> HybridCompressor::decompressWithHuffmanPlus(const std::vector<uint8_t>& data) const {
    auto huffmanDecompressed = huffman_->decompress(data);
    return postprocessFromHuffman(huffmanDecompressed);
}

std::vector<uint8_t> HybridCompressor::applyRLE(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    std::vector<uint8_t> result;
    result.reserve(data.size());
    
    size_t i = 0;
    while (i < data.size()) {
        uint8_t current = data[i];
        size_t count = 1;
        
        // Count consecutive identical bytes
        while (i + count < data.size() && data[i + count] == current && count < 255) {
            count++;
        }
        
        if (count >= 3) {
            // Encode as run: [0xFF][count][value]
            result.push_back(0xFF);
            result.push_back(static_cast<uint8_t>(count));
            result.push_back(current);
        } else {
            // Encode as literal bytes
            for (size_t j = 0; j < count; ++j) {
                result.push_back(current);
            }
        }
        
        i += count;
    }
    
    return result;
}

std::vector<uint8_t> HybridCompressor::removeRLE(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    std::vector<uint8_t> result;
    result.reserve(data.size() * 2);
    
    size_t i = 0;
    while (i < data.size()) {
        uint8_t current = data[i++];
        
        if (current == 0xFF && i + 1 < data.size()) {
            // Run-length encoding
            uint8_t count = data[i++];
            uint8_t value = data[i++];
            
            result.insert(result.end(), count, value);
        } else {
            // Literal byte
            result.push_back(current);
        }
    }
    
    return result;
}

std::vector<uint8_t> HybridCompressor::preprocessForHuffman(const std::vector<uint8_t>& data) const {
    // Apply byte difference transformation to improve entropy
    std::vector<uint8_t> result;
    result.reserve(data.size());
    
    if (data.empty()) {
        return result;
    }
    
    result.push_back(data[0]);
    for (size_t i = 1; i < data.size(); ++i) {
        result.push_back(data[i] ^ data[i-1]);
    }
    
    return result;
}

std::vector<uint8_t> HybridCompressor::postprocessFromHuffman(const std::vector<uint8_t>& data) const {
    // Reverse the byte difference transformation
    std::vector<uint8_t> result;
    result.reserve(data.size());
    
    if (data.empty()) {
        return result;
    }
    
    result.push_back(data[0]);
    for (size_t i = 1; i < data.size(); ++i) {
        result.push_back(data[i] ^ result[i-1]);
    }
    
    return result;
}

} // namespace compression
