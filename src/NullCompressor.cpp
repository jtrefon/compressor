#include <compression/NullCompressor.hpp>

namespace compression {

std::vector<std::byte> NullCompressor::compress(const std::vector<std::byte>& data) const {
    // No compression, just return the original data.
    return data;
}

std::vector<std::byte> NullCompressor::decompress(const std::vector<std::byte>& data) const {
    // No decompression, just return the original data.
    return data;
}

} // namespace compression 