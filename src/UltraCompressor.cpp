#include <compression/UltraCompressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/OptimizedCompressor.hpp>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <utility>
 
namespace compression {
 
UltraCompressor::UltraCompressor() 
    : lz77_(std::make_unique<Lz77Compressor>(65536, 3, 258, false, true, true)),
      bwt_(std::make_unique<BwtCompressor>()),
      optimized_(std::make_unique<OptimizedCompressor>()) {
}
 
UltraCompressor::~UltraCompressor() = default;
 
std::vector<uint8_t> UltraCompressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
 
    auto bwt_transformed = bwt_->transform(data);

    auto lz77_compressed = lz77_->compress(bwt_transformed);
 
    // No outer Huffman for stability; LZ77 on BWT pipeline output round-trips robustly
    constexpr uint32_t wrappedHeaderBase = 0xFFFFFFF0u;
    constexpr size_t wrapperOverhead = 4;
 
    std::vector<uint8_t> best_payload = std::move(lz77_compressed);
    uint8_t best_strategy = 0;
    size_t best_total_size = best_payload.size();
 
    auto optimized_payload = optimized_->compress(data);
    size_t optimized_total_size = optimized_payload.size() + wrapperOverhead;
    if (optimized_total_size < best_total_size) {
        best_total_size = optimized_total_size;
        best_payload = std::move(optimized_payload);
        best_strategy = 1;
    }
 
    auto bwt_payload = bwt_->compress(data);
    size_t bwt_total_size = bwt_payload.size() + wrapperOverhead;
    if (bwt_total_size < best_total_size) {
        best_total_size = bwt_total_size;
        best_payload = std::move(bwt_payload);
        best_strategy = 2;
    }
 
    if (best_strategy == 0) {
        return best_payload;
    }
 
    std::vector<uint8_t> result;
    result.reserve(best_payload.size() + wrapperOverhead);
    const uint32_t header = wrappedHeaderBase | static_cast<uint32_t>(best_strategy);
    result.push_back(static_cast<uint8_t>(header & 0xFF));
    result.push_back(static_cast<uint8_t>((header >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>((header >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((header >> 24) & 0xFF));
    result.insert(result.end(), best_payload.begin(), best_payload.end());
    return result;
}
 
std::vector<uint8_t> UltraCompressor::decompress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
 
    if (data.size() >= 4) {
        const uint32_t header = static_cast<uint32_t>(data[0]) |
            (static_cast<uint32_t>(data[1]) << 8) |
            (static_cast<uint32_t>(data[2]) << 16) |
            (static_cast<uint32_t>(data[3]) << 24);
 
        if ((header & 0xFFFFFFF0u) == 0xFFFFFFF0u) {
            uint8_t strategy = static_cast<uint8_t>(header & 0x0F);
            std::vector<uint8_t> payload(data.begin() + 4, data.end());
 
            switch (strategy) {
                case 0: {
                    auto lz77_decompressed = lz77_->decompress(payload);
                    return bwt_->inverseTransform(lz77_decompressed);
                }
                case 1: {
                    return optimized_->decompress(payload);
                }
                case 2: {
                    return bwt_->decompress(payload);
                }
                default:
                    throw std::runtime_error("Unknown Ultra compression strategy");
            }
        }
    }
 
    auto lz77_decompressed = lz77_->decompress(data);
    return bwt_->inverseTransform(lz77_decompressed);
}
 
} // namespace compression

