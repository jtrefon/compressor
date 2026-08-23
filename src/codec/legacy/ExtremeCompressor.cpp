#include <compression/codec/legacy/ExtremeCompressor.hpp>
#include <compression/codec/legacy/BwtCompressor.hpp>
#include <compression/codec/legacy/HuffmanCompressor.hpp>
#include <compression/codec/legacy/Lz77Compressor.hpp>
#include <compression/codec/legacy/OptimizedCompressor.hpp>
#include <algorithm>
#include <vector>
#include <future>
#include <thread>

namespace compression {

namespace {
constexpr size_t kWindow = 65536;
constexpr size_t kMinMatch = 3;
constexpr size_t kMaxMatch = 258;
} // namespace

ExtremeCompressor::ExtremeCompressor() = default;

std::vector<uint8_t> ExtremeCompressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    // Launch strategies in parallel

    auto f1 = std::async(std::launch::async, &ExtremeCompressor::applyBwtLz77Huffman, this, std::cref(data));
    auto f2 = std::async(std::launch::async, &ExtremeCompressor::applyLz77BwtHuffman, this, std::cref(data));
    auto f3 = std::async(std::launch::async, &ExtremeCompressor::applyPreprocessBwtLz77Huffman, this, std::cref(data));
    auto f4 = std::async(std::launch::async, [&data]() {
        OptimizedCompressor optimized;
        return optimized.compress(data);
    });
    auto f5 = std::async(std::launch::async, [&data]() {
        BwtCompressor bwt;
        return bwt.compress(data);
    });

    // Collect results
    std::vector<std::pair<std::vector<uint8_t>, std::string>> candidates;
    candidates.push_back({f1.get(), "BWT+LZ77+Huffman"});
    candidates.push_back({f2.get(), "LZ77+BWT+Huffman"});
    candidates.push_back({f3.get(), "Preprocess+BWT+LZ77+Huffman"});
    candidates.push_back({f4.get(), "Optimized"});
    candidates.push_back({f5.get(), "BWT"});
    
    // Find the best compression
    auto best_it = std::min_element(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) {
            return a.first.size() < b.first.size();
        });

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
            case 2: return reversePreprocessBwtLz77Huffman(compressed_data);
            case 3: {
                OptimizedCompressor optimized;
                return optimized.decompress(compressed_data);
            }
            case 4: {
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

// Strategy implementations — each uses local compressor instances so that a
// shared ExtremeCompressor can be called from multiple threads concurrently.
std::vector<uint8_t> ExtremeCompressor::applyBwtLz77Huffman(const std::vector<uint8_t>& data) const {
    Lz77Compressor lz77(kWindow, kMinMatch, kMaxMatch, false, true, true);
    HuffmanCompressor huffman;
    BwtCompressor bwt;
    auto bwt_transformed = bwt.transform(data);
    auto lz77_compressed = lz77.compress(bwt_transformed);
    return huffman.compress(lz77_compressed);
}

std::vector<uint8_t> ExtremeCompressor::applyLz77BwtHuffman(const std::vector<uint8_t>& data) const {
    Lz77Compressor lz77(kWindow, kMinMatch, kMaxMatch, false, true, true);
    HuffmanCompressor huffman;
    BwtCompressor bwt;
    auto lz77_compressed = lz77.compress(data);
    auto bwt_transformed = bwt.transform(lz77_compressed);
    return huffman.compress(bwt_transformed);
}

std::vector<uint8_t> ExtremeCompressor::applyPreprocessBwtLz77Huffman(const std::vector<uint8_t>& data) const {
    Lz77Compressor lz77(kWindow, kMinMatch, kMaxMatch, false, true, true);
    HuffmanCompressor huffman;
    BwtCompressor bwt;
    auto preprocessed = preprocessData(data);
    auto bwt_transformed = bwt.transform(preprocessed);
    auto lz77_compressed = lz77.compress(bwt_transformed);
    return huffman.compress(lz77_compressed);
}

// Reverse implementations
std::vector<uint8_t> ExtremeCompressor::reverseBwtLz77Huffman(const std::vector<uint8_t>& data) const {
    Lz77Compressor lz77(kWindow, kMinMatch, kMaxMatch, false, true, true);
    HuffmanCompressor huffman;
    BwtCompressor bwt;
    auto huffman_decompressed = huffman.decompress(data);
    auto lz77_decompressed = lz77.decompress(huffman_decompressed);
    return bwt.inverseTransform(lz77_decompressed);
}

std::vector<uint8_t> ExtremeCompressor::reverseLz77BwtHuffman(const std::vector<uint8_t>& data) const {
    Lz77Compressor lz77(kWindow, kMinMatch, kMaxMatch, false, true, true);
    HuffmanCompressor huffman;
    BwtCompressor bwt;
    auto huffman_decompressed = huffman.decompress(data);
    auto bwt_inversed = bwt.inverseTransform(huffman_decompressed);
    return lz77.decompress(bwt_inversed);
}

std::vector<uint8_t> ExtremeCompressor::reversePreprocessBwtLz77Huffman(const std::vector<uint8_t>& data) const {
    Lz77Compressor lz77(kWindow, kMinMatch, kMaxMatch, false, true, true);
    HuffmanCompressor huffman;
    BwtCompressor bwt;
    auto huffman_decompressed = huffman.decompress(data);
    auto lz77_decompressed = lz77.decompress(huffman_decompressed);
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
