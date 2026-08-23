#pragma once

#include <compression/ICompressor.hpp>
#include <compression/events/EventBus.hpp>
#include <compression/core/Executor.hpp>

#include <cstdint>
#include <memory>

namespace compression {
namespace codec {

/**
 * @brief Chunked parallel wrapper implementing ICompressor (Decorator).
 *
 * Splits input into chunks, compresses/decompresses each on an injected
 * IExecutor (port — tests use InlineExecutor for determinism, production uses
 * ThreadPool), and frames the result with the standard 1.x container so the
 * bytes interoperate with the rest of the library. Emits per-chunk progress
 * events on the injected EventBus.
 */
class ParallelCodecDecorator final : public ICompressor {
public:
  /**
   * @param base codec for the single-chunk path.
   * @param algoId stable codec id (stored in the frame, also used to build
   * per-chunk codecs).
   * @param executor task executor (non-owning). May be null when
   * @p chunkCount <= 1.
   * @param chunkCount number of chunks; 0 = auto (hardware concurrency).
   */
  ParallelCodecDecorator(std::unique_ptr<ICompressor> base,
                         format::AlgorithmID algoId,
                         core::IExecutor *executor,
                         std::size_t chunkCount = 0,
                         std::shared_ptr<events::EventBus> events = nullptr);

  std::vector<uint8_t> compress(const std::vector<uint8_t> &data) const override;
  std::vector<uint8_t> decompress(const std::vector<uint8_t> &data) const override;

private:
  void publish(events::EventType type, format::AlgorithmID codec, uint64_t in,
               uint64_t out, uint8_t progressPct) const;

  mutable std::unique_ptr<ICompressor> base_;
  format::AlgorithmID algoId_;
  core::IExecutor *executor_;
  std::size_t chunkCount_;
  std::shared_ptr<events::EventBus> events_;
};

} // namespace codec
} // namespace compression
