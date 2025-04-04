#pragma once

#include "ICompressor.hpp"

namespace compression {

/**
 * @brief Implements ICompressor with a null compression algorithm.
 *
 * A simple implementation that does no compression - just returns 
 * the original data. Useful for testing or as a baseline.
 */
class NullCompressor final : public ICompressor {
public:
    /**
     * @brief Does not compress the data, simply returns the original.
     *
     * @param data The raw data to "compress".
     * @return std::vector<uint8_t> The original data.
     */
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;

    /**
     * @brief Does not decompress, simply returns the original.
     *
     * @param data The data to "decompress".
     * @return std::vector<uint8_t> The original data.
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) const override;
};

} // namespace compression 