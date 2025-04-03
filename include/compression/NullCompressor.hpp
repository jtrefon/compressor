#pragma once

#include "ICompressor.hpp"

namespace compression {

/**
 * @brief A Null Object implementation of ICompressor.
 *
 * This class implements the ICompressor interface but performs
 * no actual compression or decompression. It simply returns the
 * input data unchanged.
 */
class NullCompressor final : public ICompressor {
public:
    /**
     * @brief Returns the input data without modification.
     *
     * @param data The data to "compress".
     * @return std::vector<std::byte> The unchanged input data.
     */
    std::vector<std::byte> compress(const std::vector<std::byte>& data) const override;

    /**
     * @brief Returns the input data without modification.
     *
     * @param data The data to "decompress".
     * @return std::vector<std::byte> The unchanged input data.
     */
    std::vector<std::byte> decompress(const std::vector<std::byte>& data) const override;
};

} // namespace compression 