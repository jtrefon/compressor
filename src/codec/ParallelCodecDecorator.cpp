#include <compression/codec/ParallelCodecDecorator.hpp>

#include <compression/Crc32.hpp>
#include <compression/SystemInfo.hpp>
#include <compression/codec/CodecRegistry.hpp>
#include <compression/core/ByteView.hpp>
#include <compression/core/Errors.hpp>

#include <algorithm>
#include <cstdint>
#include <future>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace compression {
namespace codec {

namespace {

constexpr std::size_t kMaxChunks = 1024;
constexpr uint64_t kMaxReserve = 1ull << 30;

std::unique_ptr<ICompressor> makeCodec(format::AlgorithmID id) {
  return CodecRegistry::instance().create(id);
}

} // namespace

ParallelCodecDecorator::ParallelCodecDecorator(
    std::unique_ptr<ICompressor> base, format::AlgorithmID algoId,
    core::IExecutor *executor, std::size_t chunkCount,
    std::shared_ptr<app::EventBus> events)
    : base_(std::move(base)), algoId_(algoId), executor_(executor),
      chunkCount_(chunkCount), events_(std::move(events)) {}

void ParallelCodecDecorator::publish(app::EventType type,
                                     format::AlgorithmID codec, uint64_t in,
                                     uint64_t out, uint8_t progressPct) const {
  if (events_) {
    events_->publish(app::CompressionEvent{type, codec, in, out, progressPct});
  }
}

std::vector<uint8_t>
ParallelCodecDecorator::compress(const std::vector<uint8_t> &data) const {
  publish(app::EventType::OperationStarted, algoId_, data.size(), 0, 0);

  const std::size_t chunkCount =
      (chunkCount_ == 0) ? std::max<std::size_t>(getHardwareThreads(), 1)
                         : chunkCount_;

  if (chunkCount <= 1 || data.empty()) {
    format::FileHeader header;
    header.algorithmId = algoId_;
    header.originalSize = data.size();
    header.originalChecksum = utils::crc32Calculator.calculate(data);
    header.chunkCount = 1;
    header.chunkSize = static_cast<uint32_t>(data.size());
    std::vector<uint8_t> compressed = base_->compress(data);
    header.compressedSizes = {static_cast<uint32_t>(compressed.size())};
    std::vector<uint8_t> headerBytes = format::serializeHeader(header);
    std::vector<uint8_t> output;
    output.reserve(headerBytes.size() + compressed.size());
    output.insert(output.end(), headerBytes.begin(), headerBytes.end());
    output.insert(output.end(), compressed.begin(), compressed.end());
    publish(app::EventType::OperationCompleted, algoId_, data.size(),
            output.size(), 100);
    return output;
  }

  if (executor_ == nullptr) {
    throw std::logic_error(
        "ParallelCodecDecorator: executor required for multi-chunk compress");
  }

  const std::size_t n = std::min(chunkCount, data.size());
  const std::size_t baseChunk = data.size() / n;
  const std::size_t remainder = data.size() % n;

  std::vector<std::future<std::vector<uint8_t>>> futures(n);
  std::vector<uint32_t> sizes(n);
  std::vector<std::vector<uint8_t>> results(n);
  std::vector<std::size_t> rawChunkSizes(n);

  std::size_t offset = 0;
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t sz = baseChunk + (i < remainder ? 1 : 0);
    rawChunkSizes[i] = sz;
    std::vector<uint8_t> chunk(data.begin() + offset,
                               data.begin() + offset + sz);
    offset += sz;
    futures[i] = executor_->submit(
        [algoId = algoId_, c = std::move(chunk)]() mutable {
          return makeCodec(algoId)->compress(c);
        });
  }

  for (std::size_t i = 0; i < n; ++i) {
    results[i] = futures[i].get();
    sizes[i] = static_cast<uint32_t>(results[i].size());
    publish(app::EventType::ChunkProgress, algoId_, rawChunkSizes[i],
            results[i].size(),
            static_cast<uint8_t>((i + 1) * 100 / n));
  }

  format::FileHeader header;
  header.algorithmId = algoId_;
  header.originalSize = data.size();
  header.originalChecksum = utils::crc32Calculator.calculate(data);
  header.chunkCount = static_cast<uint32_t>(n);
  header.chunkSize = static_cast<uint32_t>(baseChunk);
  header.compressedSizes = sizes;
  std::vector<uint8_t> headerBytes = format::serializeHeader(header);

  std::vector<uint8_t> output;
  const std::size_t total =
      std::accumulate(sizes.begin(), sizes.end(), std::size_t{0});
  output.reserve(headerBytes.size() + total);
  output.insert(output.end(), headerBytes.begin(), headerBytes.end());
  for (const auto &r : results) {
    output.insert(output.end(), r.begin(), r.end());
  }
  publish(app::EventType::OperationCompleted, algoId_, data.size(),
          output.size(), 100);
  return output;
}

std::vector<uint8_t>
ParallelCodecDecorator::decompress(const std::vector<uint8_t> &data) const {
  format::FileHeader header = format::deserializeHeader(data);
  const std::size_t headerSize = format::serializedHeaderSize(header);

  if (data.size() < headerSize) {
    throw core::CorruptDataError("Input too small for header");
  }

  const std::size_t chunkCount = header.chunkCount;
  if (chunkCount == 0 || chunkCount > kMaxChunks) {
    throw core::CorruptDataError("Invalid chunk count in header");
  }

  std::vector<uint8_t> output;
  if (chunkCount == 1) {
    const core::ByteView payload(data.data() + headerSize,
                                 data.size() - headerSize);
    output = makeCodec(header.algorithmId)
                 ->decompress(std::vector<uint8_t>(payload.begin(),
                                                   payload.end()));
  } else {
    if (executor_ == nullptr) {
      throw std::logic_error(
          "ParallelCodecDecorator: executor required for multi-chunk "
          "decompress");
    }
    std::vector<std::future<std::vector<uint8_t>>> futures(chunkCount);
    std::vector<std::vector<uint8_t>> results(chunkCount);
    std::size_t offset = headerSize;
    for (uint32_t i = 0; i < chunkCount; ++i) {
      const uint32_t sz = header.compressedSizes[i];
      if (offset + sz > data.size()) {
        throw core::CorruptDataError("Truncated chunk data");
      }
      std::vector<uint8_t> chunk(data.begin() + offset,
                                 data.begin() + offset + sz);
      offset += sz;
      futures[i] = executor_->submit(
          [algoId = header.algorithmId, c = std::move(chunk)]() mutable {
            return makeCodec(algoId)->decompress(c);
          });
    }
    for (uint32_t i = 0; i < chunkCount; ++i) {
      results[i] = futures[i].get();
      publish(app::EventType::ChunkProgress, header.algorithmId, 0,
              results[i].size(),
              static_cast<uint8_t>((i + 1) * 100 / chunkCount));
    }
    if (header.originalSize <= kMaxReserve) {
      output.reserve(static_cast<std::size_t>(header.originalSize));
    }
    for (const auto &r : results) {
      output.insert(output.end(), r.begin(), r.end());
    }
  }

  if (utils::crc32Calculator.calculate(output) != header.originalChecksum) {
    throw core::CorruptDataError("Checksum mismatch: data corrupted");
  }
  publish(app::EventType::OperationCompleted, header.algorithmId, data.size(),
          output.size(), 100);
  return output;
}

} // namespace codec
} // namespace compression
