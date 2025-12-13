#include <compression/ExtremeCompressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/OptimizedCompressor.hpp>
#include <algorithm>
#include <map>
#include <vector>
#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <future>
#include <thread>

namespace compression {

ExtremeCompressor::ExtremeCompressor() 
    : huffman_(std::make_unique<HuffmanCompressor>()),
      lz77_(std::make_unique<Lz77Compressor>(65536, 3, 258, false, true, true)) {
}

ExtremeCompressor::~ExtremeCompressor() = default;

std::vector<uint8_t> ExtremeCompressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    std::cout << "🚀 EXTREME COMPRESSION MODE - Maximum Ratio (Parallel)" << std::endl;
    
    // Launch strategies in parallel
    std::cout << "🔄 Launching 7 compression strategies in parallel..." << std::endl;

    auto f1 = std::async(std::launch::async, &ExtremeCompressor::applyBwtLz77Huffman, this, std::cref(data));
    auto f2 = std::async(std::launch::async, &ExtremeCompressor::applyLz77BwtHuffman, this, std::cref(data));
    auto f3 = std::async(std::launch::async, &ExtremeCompressor::applyBwtHuffmanLz77, this, std::cref(data));
    auto f4 = std::async(std::launch::async, &ExtremeCompressor::applyDoubleBwtHuffman, this, std::cref(data));
    auto f5 = std::async(std::launch::async, &ExtremeCompressor::applyPreprocessBwtLz77Huffman, this, std::cref(data));
    auto f6 = std::async(std::launch::async, [&data]() {
        OptimizedCompressor optimized;
        return optimized.compress(data);
    });
    auto f7 = std::async(std::launch::async, [&data]() {
        BwtCompressor bwt;
        return bwt.compress(data);
    });

    // Collect results
    std::vector<std::pair<std::vector<uint8_t>, std::string>> candidates;
    candidates.push_back({f1.get(), "BWT+LZ77+Huffman"});
    candidates.push_back({f2.get(), "LZ77+BWT+Huffman"});
    candidates.push_back({f3.get(), "BWT+Huffman+LZ77"});
    candidates.push_back({f4.get(), "DoubleBWT+Huffman"});
    candidates.push_back({f5.get(), "Preprocess+BWT+LZ77+Huffman"});
    candidates.push_back({f6.get(), "Optimized"});
    candidates.push_back({f7.get(), "BWT"});
    
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
            case 5: {
                OptimizedCompressor optimized;
                return optimized.decompress(compressed_data);
            }
            case 6: {
                BwtCompressor bwt;
                return bwt.decompress(compressed_data);
            }
            default:
                throw std::runtime_error("Unknown compression strategy");
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Extreme decompression failed: ") + e.what());
    }
}

// Strategy implementations
std::vector<uint8_t> ExtremeCompressor::applyBwtLz77Huffman(const std::vector<uint8_t>& data) const {
    BwtCompressor bwt;
    auto bwt_transformed = bwt.transform(data);
    auto lz77_compressed = lz77_->compress(bwt_transformed);
    return huffman_->compress(lz77_compressed);
}

std::vector<uint8_t> ExtremeCompressor::applyLz77BwtHuffman(const std::vector<uint8_t>& data) const {
    auto lz77_compressed = lz77_->compress(data);
    BwtCompressor bwt;
    auto bwt_transformed = bwt.transform(lz77_compressed);
    return huffman_->compress(bwt_transformed);
}

std::vector<uint8_t> ExtremeCompressor::applyBwtHuffmanLz77(const std::vector<uint8_t>& data) const {
    BwtCompressor bwt;
    auto bwt_transformed = bwt.transform(data);
    auto huffman_compressed = huffman_->compress(bwt_transformed);
    return lz77_->compress(huffman_compressed);
}

std::vector<uint8_t> ExtremeCompressor::applyDoubleBwtHuffman(const std::vector<uint8_t>& data) const {
    BwtCompressor bwt;
    auto bwt1 = bwt.transform(data);
    auto bwt2 = bwt.transform(bwt1);
    return huffman_->compress(bwt2);
}

std::vector<uint8_t> ExtremeCompressor::applyPreprocessBwtLz77Huffman(const std::vector<uint8_t>& data) const {
    auto preprocessed = preprocessData(data);
    BwtCompressor bwt;
    auto bwt_transformed = bwt.transform(preprocessed);
    auto lz77_compressed = lz77_->compress(bwt_transformed);
    return huffman_->compress(lz77_compressed);
}

// Reverse implementations
std::vector<uint8_t> ExtremeCompressor::reverseBwtLz77Huffman(const std::vector<uint8_t>& data) const {
    auto huffman_decompressed = huffman_->decompress(data);
    auto lz77_decompressed = lz77_->decompress(huffman_decompressed);
    BwtCompressor bwt;
    return bwt.inverseTransform(lz77_decompressed);
}

std::vector<uint8_t> ExtremeCompressor::reverseLz77BwtHuffman(const std::vector<uint8_t>& data) const {
    auto huffman_decompressed = huffman_->decompress(data);
    BwtCompressor bwt;
    auto bwt_inversed = bwt.inverseTransform(huffman_decompressed);
    return lz77_->decompress(bwt_inversed);
}

std::vector<uint8_t> ExtremeCompressor::reverseBwtHuffmanLz77(const std::vector<uint8_t>& data) const {
    auto lz77_decompressed = lz77_->decompress(data);
    auto huffman_decompressed = huffman_->decompress(lz77_decompressed);
    BwtCompressor bwt;
    return bwt.inverseTransform(huffman_decompressed);
}

std::vector<uint8_t> ExtremeCompressor::reverseDoubleBwtHuffman(const std::vector<uint8_t>& data) const {
    auto huffman_decompressed = huffman_->decompress(data);
    BwtCompressor bwt;
    auto bwt1 = bwt.inverseTransform(huffman_decompressed);
    return bwt.inverseTransform(bwt1);
}

std::vector<uint8_t> ExtremeCompressor::reversePreprocessBwtLz77Huffman(const std::vector<uint8_t>& data) const {
    auto huffman_decompressed = huffman_->decompress(data);
    auto lz77_decompressed = lz77_->decompress(huffman_decompressed);
    BwtCompressor bwt;
    auto bwt_inversed = bwt.inverseTransform(lz77_decompressed);
    return postprocessData(bwt_inversed);
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
