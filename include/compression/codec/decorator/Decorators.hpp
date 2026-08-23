#pragma once

#include <compression/ICompressor.hpp>
#include <compression/events/EventBus.hpp>

#include <chrono>
#include <cstdint>
#include <memory>

namespace compression {
namespace codec {
namespace decorator {

/**
 * @brief Measures wall-clock time of the wrapped codec (Decorator).
 * Exposes the elapsed time of the last operation.
 */
class TimedCodec final : public ICompressor {
public:
  explicit TimedCodec(std::unique_ptr<ICompressor> inner);

  std::vector<uint8_t> compress(const std::vector<uint8_t> &data) const override;
  std::vector<uint8_t> decompress(const std::vector<uint8_t> &data) const override;

  std::chrono::milliseconds lastElapsed() const { return lastElapsed_; }

private:
  mutable std::unique_ptr<ICompressor> inner_;
  mutable std::chrono::milliseconds lastElapsed_{0};
};

/**
 * @brief Appends a CRC32 trailer to compressed output and verifies it on
 * decompress (Decorator). Corrupted payloads throw CorruptDataError.
 */
class ChecksummedCodec final : public ICompressor {
public:
  explicit ChecksummedCodec(std::unique_ptr<ICompressor> inner);

  std::vector<uint8_t> compress(const std::vector<uint8_t> &data) const override;
  std::vector<uint8_t> decompress(const std::vector<uint8_t> &data) const override;

private:
  mutable std::unique_ptr<ICompressor> inner_;
};

/**
 * @brief Emits operation events for the wrapped codec (Decorator + Observer).
 */
class ProgressCodec final : public ICompressor {
public:
  ProgressCodec(std::unique_ptr<ICompressor> inner,
                std::shared_ptr<events::EventBus> events);

  std::vector<uint8_t> compress(const std::vector<uint8_t> &data) const override;
  std::vector<uint8_t> decompress(const std::vector<uint8_t> &data) const override;

private:
  void publish(events::EventType type, uint64_t in, uint64_t out,
               uint8_t progressPct) const;

  mutable std::unique_ptr<ICompressor> inner_;
  std::shared_ptr<events::EventBus> events_;
};

} // namespace decorator
} // namespace codec
} // namespace compression
