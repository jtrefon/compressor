#include <compression/ExtremeCompressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <algorithm>
#include <map>
#include <vector>
#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>

namespace compression {

ExtremeCompressor::ExtremeCompressor() 
    : bwt_(std::make_unique<BwtCompressor>()),
      huffman_(std::make_unique<HuffmanCompressor>()),
      lz77_(std::make_unique<Lz77Compressor>(65536, 3, 258, false, true, true)) {
}

ExtremeCompressor::~ExtremeCompressor() = default;

std::vector<uint8_t> ExtremeCompressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    std::cout << "🚀 EXTREME COMPRESSION MODE - Maximum Ratio" << std::endl;
    
    // Try multiple compression strategies and pick the best
    std::vector<std::pair<std::vector<uint8_t>, std::string>> candidates;
    
    // Strategy 1: BWT + LZ77 + Huffman
    std::cout << "🔄 Strategy 1: BWT → LZ77 → Huffman..." << std::endl;
    auto strategy1 = applyBwtLz77Huffman(data);
    candidates.push_back({strategy1, "BWT+LZ77+Huffman"});
    
    // Strategy 2: LZ77 + BWT + Huffman  
    std::cout << "🔄 Strategy 2: LZ77 → BWT → Huffman..." << std::endl;
    auto strategy2 = applyLz77BwtHuffman(data);
    candidates.push_back({strategy2, "LZ77+BWT+Huffman"});
    
    // Strategy 3: BWT + Huffman + LZ77
    std::cout << "🔄 Strategy 3: BWT → Huffman → LZ77..." << std::endl;
    auto strategy3 = applyBwtHuffmanLz77(data);
    candidates.push_back({strategy3, "BWT+Huffman+LZ77"});
    
    // Strategy 4: Double BWT + Huffman
    std::cout << "🔄 Strategy 4: Double BWT → Huffman..." << std::endl;
    auto strategy4 = applyDoubleBwtHuffman(data);
    candidates.push_back({strategy4, "DoubleBWT+Huffman"});
    
    // Strategy 5: Preprocessing + BWT + LZ77 + Huffman
    std::cout << "🔄 Strategy 5: Preprocess → BWT → LZ77 → Huffman..." << std::endl;
    auto strategy5 = applyPreprocessBwtLz77Huffman(data);
    candidates.push_back({strategy5, "Preprocess+BWT+LZ77+Huffman"});
    
    // Find the best compression
    auto best_it = std::min_element(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) {
            return a.first.size() < b.first.size();
        });
    
    std::cout << "🏆 Best strategy: " << best_it->second 
              << " (size: " << best_it->first.size() << " bytes, ratio: " 
              << std::fixed << std::setprecision(1) 
              << (100.0 * best_it->first.size() / data.size()) << "%)" << std::endl;
    
    // Add method marker and strategy info
    std::vector<uint8_t> result;
    result.push_back(0xFE); // Extreme compressor marker
    
    // Add strategy index
    size_t strategy_index = std::distance(candidates.begin(), best_it);
    result.push_back(static_cast<uint8_t>(strategy_index));
    
    result.insert(result.end(), best_it->first.begin(), best_it->first.end());
    
    return result;
}

std::vector<uint8_t> ExtremeCompressor::decompress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    if (data[0] != 0xFE) {
        throw std::runtime_error("Invalid Extreme compressor data");
    }
    
    if (data.size() < 2) {
        throw std::runtime_error("Invalid Extreme compressor data: missing strategy index");
    }
    
    uint8_t strategy_index = data[1];
    std::vector<uint8_t> compressed_data(data.begin() + 2, data.end());
    
    try {
        switch (strategy_index) {
            case 0: return reverseBwtLz77Huffman(compressed_data);
            case 1: return reverseLz77BwtHuffman(compressed_data);
            case 2: return reverseBwtHuffmanLz77(compressed_data);
            case 3: return reverseDoubleBwtHuffman(compressed_data);
            case 4: return reversePreprocessBwtLz77Huffman(compressed_data);
            default:
                throw std::runtime_error("Unknown compression strategy");
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Extreme decompression failed: ") + e.what());
    }
}

// Strategy implementations
std::vector<uint8_t> ExtremeCompressor::applyBwtLz77Huffman(const std::vector<uint8_t>& data) const {
    auto bwt_compressed = bwt_->compress(data);
    auto lz77_compressed = lz77_->compress(bwt_compressed);
    return huffman_->compress(lz77_compressed);
}

std::vector<uint8_t> ExtremeCompressor::applyLz77BwtHuffman(const std::vector<uint8_t>& data) const {
    auto lz77_compressed = lz77_->compress(data);
    auto bwt_compressed = bwt_->compress(lz77_compressed);
    return huffman_->compress(bwt_compressed);
}

std::vector<uint8_t> ExtremeCompressor::applyBwtHuffmanLz77(const std::vector<uint8_t>& data) const {
    auto bwt_compressed = bwt_->compress(data);
    auto huffman_compressed = huffman_->compress(bwt_compressed);
    return lz77_->compress(huffman_compressed);
}

std::vector<uint8_t> ExtremeCompressor::applyDoubleBwtHuffman(const std::vector<uint8_t>& data) const {
    auto bwt1 = bwt_->compress(data);
    auto bwt2 = bwt_->compress(bwt1);
    return huffman_->compress(bwt2);
}

std::vector<uint8_t> ExtremeCompressor::applyPreprocessBwtLz77Huffman(const std::vector<uint8_t>& data) const {
    auto preprocessed = preprocessData(data);
    auto bwt_compressed = bwt_->compress(preprocessed);
    auto lz77_compressed = lz77_->compress(bwt_compressed);
    return huffman_->compress(lz77_compressed);
}

// Reverse implementations
std::vector<uint8_t> ExtremeCompressor::reverseBwtLz77Huffman(const std::vector<uint8_t>& data) const {
    auto huffman_decompressed = huffman_->decompress(data);
    auto lz77_decompressed = lz77_->decompress(huffman_decompressed);
    return bwt_->decompress(lz77_decompressed);
}

std::vector<uint8_t> ExtremeCompressor::reverseLz77BwtHuffman(const std::vector<uint8_t>& data) const {
    auto huffman_decompressed = huffman_->decompress(data);
    auto bwt_decompressed = bwt_->decompress(huffman_decompressed);
    return lz77_->decompress(bwt_decompressed);
}

std::vector<uint8_t> ExtremeCompressor::reverseBwtHuffmanLz77(const std::vector<uint8_t>& data) const {
    auto lz77_decompressed = lz77_->decompress(data);
    auto huffman_decompressed = huffman_->decompress(lz77_decompressed);
    return bwt_->decompress(huffman_decompressed);
}

std::vector<uint8_t> ExtremeCompressor::reverseDoubleBwtHuffman(const std::vector<uint8_t>& data) const {
    auto huffman_decompressed = huffman_->decompress(data);
    auto bwt1 = bwt_->decompress(huffman_decompressed);
    return bwt_->decompress(bwt1);
}

std::vector<uint8_t> ExtremeCompressor::reversePreprocessBwtLz77Huffman(const std::vector<uint8_t>& data) const {
    auto huffman_decompressed = huffman_->decompress(data);
    auto lz77_decompressed = lz77_->decompress(huffman_decompressed);
    auto bwt_decompressed = bwt_->decompress(lz77_decompressed);
    return postprocessData(bwt_decompressed);
}

// Preprocessing for better compression
std::vector<uint8_t> ExtremeCompressor::preprocessData(const std::vector<uint8_t>& data) const {
    std::vector<uint8_t> result;
    result.reserve(data.size());
    
    if (data.empty()) return result;
    
    // Apply byte difference transformation
    result.push_back(data[0]);
    for (size_t i = 1; i < data.size(); ++i) {
        result.push_back(data[i] ^ data[i-1]);
    }
    
    return result;
}

std::vector<uint8_t> ExtremeCompressor::postprocessData(const std::vector<uint8_t>& data) const {
    std::vector<uint8_t> result;
    result.reserve(data.size());
    
    if (data.empty()) return result;
    
    // Reverse byte difference transformation
    result.push_back(data[0]);
    for (size_t i = 1; i < data.size(); ++i) {
        result.push_back(data[i] ^ result[i-1]);
    }
    
    return result;
}

} // namespace compression
