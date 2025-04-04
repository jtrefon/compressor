#pragma once

#include <vector>
#include <cstdint>

namespace compression {

// Helper class to adapt between std::byte and other byte-like types (uint8_t)
class ByteTypeAdapter {
public:
    // Convert std::vector<std::byte> to std::vector<uint8_t>
    static std::vector<uint8_t> bytesToUint8(const std::vector<uint8_t>& input) {
        return input; // No conversion needed anymore
    }
    
    // Convert std::vector<uint8_t> to std::vector<std::byte>
    static std::vector<uint8_t> uint8ToByte(const std::vector<uint8_t>& input) {
        return input; // No conversion needed anymore
    }
};

} // namespace compression 