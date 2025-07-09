#pragma once

#include "compression/ICompressor.hpp"
#include "compression/ThreadPool.hpp"
#include "compression/FileFormat.hpp"
#include "compression/SystemInfo.hpp"
#include "compression/Crc32.hpp"

#include <memory>
#include <vector>

namespace compression {

class ParallelCompressor {
public:
    ParallelCompressor(std::unique_ptr<ICompressor> base,
                       format::AlgorithmID algoId,
                       std::size_t threads = 0);

    std::vector<uint8_t> compress(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data);

private:
    std::unique_ptr<ICompressor> base_;
    format::AlgorithmID algoId_;
    std::size_t threads_;
};

} // namespace compression
