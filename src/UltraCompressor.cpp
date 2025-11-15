#include <compression/UltraCompressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <algorithm>
#include <map>
#include <vector>
#include <cmath>
#include <iostream>

namespace compression {

UltraCompressor::UltraCompressor() 
    : bwt_(std::make_unique<BwtCompressor>()),
      lz77_(std::make_unique<Lz77Compressor>(65536, 3, 258, false, true, true)) {
}

UltraCompressor::~UltraCompressor() = default;

std::vector<uint8_t> UltraCompressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }

    std::cout << "🔄 Stage 1: BWT transform..." << std::endl;
    auto bwt_compressed = bwt_->compress(data);

    std::cout << "🔄 Stage 2: LZ77 on BWT..." << std::endl;
    auto lz77_compressed = lz77_->compress(bwt_compressed);

    // No outer Huffman for stability; LZ77 on BWT pipeline output round-trips robustly
    return lz77_compressed;
}

std::vector<uint8_t> UltraCompressor::decompress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }

    auto lz77_decompressed = lz77_->decompress(data);
    return bwt_->decompress(lz77_decompressed);
}

} // namespace compression
