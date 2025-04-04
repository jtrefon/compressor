#include "compression/NullCompressor.hpp"

namespace compression {

std::vector<uint8_t> NullCompressor::compress(const std::vector<uint8_t>& data) const {
    return data; // No compression - return the original data
}

std::vector<uint8_t> NullCompressor::decompress(const std::vector<uint8_t>& data) const {
    return data; // No decompression - return the original data
}

} // namespace compression 