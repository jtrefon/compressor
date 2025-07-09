#pragma once

#include <compression/ICompressor.hpp>
#include <vector>
#include <cstdint>

namespace compression {

class BwtCompressor : public ICompressor {
public:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;

private:
    std::vector<uint8_t> bwt_transform(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> inverse_bwt_transform(const std::vector<uint8_t>& data) const;

    static std::vector<uint8_t> mtf_encode(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> mtf_decode(const std::vector<uint8_t>& data);
};

} // namespace compression
