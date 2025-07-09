#include "compression/ParallelCompressor.hpp"

#include <numeric>
#include "compression/NullCompressor.hpp"
#include "compression/RleCompressor.hpp"
#include "compression/HuffmanCompressor.hpp"
#include "compression/Lz77Compressor.hpp"
#include "compression/BwtCompressor.hpp"

namespace compression {

namespace {
std::unique_ptr<ICompressor> createCompressorById(format::AlgorithmID id) {
    using format::AlgorithmID;
    switch (id) {
        case AlgorithmID::RLE_COMPRESSOR:
            return std::make_unique<RleCompressor>();
        case AlgorithmID::NULL_COMPRESSOR:
            return std::make_unique<NullCompressor>();
        case AlgorithmID::HUFFMAN_COMPRESSOR:
            return std::make_unique<HuffmanCompressor>();
        case AlgorithmID::LZ77_COMPRESSOR:
            return std::make_unique<Lz77Compressor>(32768, 3, 258, false, true, true);
        case AlgorithmID::BWT_COMPRESSOR:
            return std::make_unique<BwtCompressor>();
        default:
            throw std::invalid_argument("Unknown AlgorithmID");
    }
}
} // namespace

ParallelCompressor::ParallelCompressor(std::unique_ptr<ICompressor> base,
                                       format::AlgorithmID algoId,
                                       std::size_t threads)
    : base_(std::move(base)), algoId_(algoId) {
    threads_ = threads ? threads : getHardwareThreads();
    if (threads_ == 0) {
        threads_ = 1;
    }
}

std::vector<uint8_t> ParallelCompressor::compress(const std::vector<uint8_t>& data) {
    if (threads_ <= 1 || data.empty()) {
        format::FileHeader header;
        header.algorithmId = algoId_;
        header.originalSize = data.size();
        header.originalChecksum = utils::crc32Calculator.calculate(data);
        header.chunkCount = 1;
        header.chunkSize = static_cast<uint32_t>(data.size());
        auto compressed = base_->compress(data);
        header.compressedSizes = { static_cast<uint32_t>(compressed.size()) };
        auto headerBytes = format::serializeHeader(header);
        std::vector<uint8_t> output;
        output.reserve(headerBytes.size() + compressed.size());
        output.insert(output.end(), headerBytes.begin(), headerBytes.end());
        output.insert(output.end(), compressed.begin(), compressed.end());
        return output;
    }

    std::size_t chunkCount = std::min(threads_, data.size() ? threads_ : 1);
    std::size_t baseChunk = data.size() / chunkCount;
    std::size_t remainder = data.size() % chunkCount;

    ThreadPool pool(chunkCount);
    std::vector<std::future<std::vector<uint8_t>>> futures(chunkCount);
    std::vector<uint32_t> sizes(chunkCount);
    std::vector<std::vector<uint8_t>> results(chunkCount);

    std::size_t offset = 0;
    for (std::size_t i = 0; i < chunkCount; ++i) {
        std::size_t sz = baseChunk + (i < remainder ? 1 : 0);
        std::vector<uint8_t> chunk(data.begin() + offset, data.begin() + offset + sz);
        offset += sz;
        futures[i] = pool.enqueue([algoId = algoId_, c = std::move(chunk)]() mutable {
            auto comp = createCompressorById(algoId);
            return comp->compress(c);
        });
    }

    for (std::size_t i = 0; i < chunkCount; ++i) {
        results[i] = futures[i].get();
        sizes[i] = static_cast<uint32_t>(results[i].size());
    }

    format::FileHeader header;
    header.algorithmId = algoId_;
    header.originalSize = data.size();
    header.originalChecksum = utils::crc32Calculator.calculate(data);
    header.chunkCount = static_cast<uint32_t>(chunkCount);
    header.chunkSize = static_cast<uint32_t>(baseChunk);
    header.compressedSizes = sizes;
    auto headerBytes = format::serializeHeader(header);

    std::vector<uint8_t> output;
    std::size_t totalCompressed = std::accumulate(sizes.begin(), sizes.end(), static_cast<std::size_t>(0));
    output.reserve(headerBytes.size() + totalCompressed);
    output.insert(output.end(), headerBytes.begin(), headerBytes.end());
    for (const auto& r : results) {
        output.insert(output.end(), r.begin(), r.end());
    }
    return output;
}

std::vector<uint8_t> ParallelCompressor::decompress(const std::vector<uint8_t>& data) {
    format::FileHeader header = format::deserializeHeader(data);
    std::size_t headerSize = format::serializedHeaderSize(header);

    if (data.size() < headerSize) {
        throw std::runtime_error("Input too small for header");
    }

    ThreadPool pool(header.chunkCount);
    std::vector<std::future<std::vector<uint8_t>>> futures(header.chunkCount);
    std::vector<std::vector<uint8_t>> results(header.chunkCount);

    std::size_t offset = headerSize;
    for (uint32_t i = 0; i < header.chunkCount; ++i) {
        uint32_t sz = header.compressedSizes[i];
        if (offset + sz > data.size()) {
            throw std::runtime_error("Truncated chunk data");
        }
        std::vector<uint8_t> chunk(data.begin() + offset, data.begin() + offset + sz);
        offset += sz;
        futures[i] = pool.enqueue([algoId = algoId_, c = std::move(chunk)]() mutable {
            auto comp = createCompressorById(algoId);
            return comp->decompress(c);
        });
    }

    std::vector<uint8_t> output;
    output.reserve(header.originalSize);
    for (uint32_t i = 0; i < header.chunkCount; ++i) {
        results[i] = futures[i].get();
        output.insert(output.end(), results[i].begin(), results[i].end());
    }
    return output;
}

} // namespace compression
