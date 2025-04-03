#include <compression/RleCompressor.hpp>
#include <stdexcept>

namespace compression {

// Simple RLE format: [Count][Value][Count][Value]...
// Count is stored as a single byte (unsigned char).
// Max run length is 255.

std::vector<std::byte> RleCompressor::compress(const std::vector<std::byte>& data) const {
    if (data.empty()) {
        return {};
    }

    std::vector<std::byte> compressedData;
    // Reserve heuristic: worst case is 2x size (alternating bytes), plus some buffer.
    compressedData.reserve(data.size() * 2 + 10);

    size_t i = 0;
    while (i < data.size()) {
        std::byte currentValue = data[i];
        uint8_t count = 1;
        size_t j = i + 1;
        while (j < data.size() && data[j] == currentValue && count < 255) {
            count++;
            j++;
        }
        compressedData.push_back(static_cast<std::byte>(count));
        compressedData.push_back(currentValue);
        i = j;
    }
    compressedData.shrink_to_fit();
    return compressedData;
}

std::vector<std::byte> RleCompressor::decompress(const std::vector<std::byte>& data) const {
    if (data.empty()) {
        return {};
    }
    // Basic validation: must have pairs of [Count][Value]
    if (data.size() % 2 != 0) {
        throw std::runtime_error("Invalid RLE data: size must be even.");
    }

    std::vector<std::byte> decompressedData;
    // We can calculate the exact size needed beforehand, but let's estimate for simplicity first.
    // A more robust version would calculate the exact size by summing counts.
    decompressedData.reserve(data.size() * 2); // Rough estimate, could be larger

    for (size_t i = 0; i < data.size(); i += 2) {
        uint8_t count = static_cast<uint8_t>(data[i]);
        if (count == 0) { // Count should never be zero in our simple format
             throw std::runtime_error("Invalid RLE data: count cannot be zero.");
        }
        std::byte value = data[i + 1];
        for (uint8_t k = 0; k < count; ++k) {
            decompressedData.push_back(value);
        }
    }
    // Note: shrink_to_fit() might not be needed if exact size was pre-calculated.
    decompressedData.shrink_to_fit(); 
    return decompressedData;
}

} // namespace compression 