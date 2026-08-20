#pragma once

#include <compression/FileFormat.hpp>
#include <compression/app/EventBus.hpp>
#include <compression/core/ByteSource.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace compression {

/**
 * @brief Client-facing compression settings (value object).
 *
 * @p threads: 0 = auto (hardware concurrency), 1 = single-threaded.
 */
struct CompressionOptions {
  format::AlgorithmID codec = format::AlgorithmID::OPTIMIZED_COMPRESSOR;
  std::size_t threads = 1;
};

/**
 * @brief Outcome of a compression operation (value object).
 */
struct CompressResult {
  uint64_t inBytes = 0;
  uint64_t outBytes = 0;
  double ratio = 0.0;
  uint32_t crc = 0;
  std::chrono::milliseconds elapsed{0};
};

/**
 * @brief Outcome of a decompression operation (value object).
 */
struct ExtractResult {
  uint64_t inBytes = 0;
  uint64_t outBytes = 0;
  uint32_t crc = 0;
  bool verified = true;
  std::chrono::milliseconds elapsed{0};
};

/**
 * @brief Facade: the single stable surface CLI and UI clients call.
 *
 * Owns no state beyond an optional event bus; all codec creation goes through
 * CodecRegistry, all framing through ParallelCompressor. Results are returned
 * as value objects — never printed here.
 */
class CompressionService {
public:
  explicit CompressionService(std::shared_ptr<app::EventBus> events = nullptr);

  // File-level operations.
  CompressResult compressFile(const std::filesystem::path &in,
                              const std::filesystem::path &out,
                              const CompressionOptions &options = {});
  ExtractResult decompressFile(const std::filesystem::path &in,
                               const std::filesystem::path &out);

  // In-memory operations (UI preview, tests, bindings).
  CompressResult compress(core::ByteView data, core::IByteSink &out,
                          const CompressionOptions &options = {});
  ExtractResult decompress(core::ByteView data, core::IByteSink &out);

  std::shared_ptr<app::EventBus> events() const { return events_; }

private:
  void publish(app::EventType type, format::AlgorithmID codec, uint64_t in,
               uint64_t out, uint8_t progressPct) const;

  std::shared_ptr<app::EventBus> events_;
};

} // namespace compression
