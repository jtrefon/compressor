#include <compression/codec/decorator/Decorators.hpp>

#include <compression/Crc32.hpp>
#include <compression/core/BinaryIO.hpp>
#include <compression/core/Errors.hpp>

#include <cstdint>
#include <utility>

namespace compression {
namespace codec {
namespace decorator {

// --- TimedCodec ---

TimedCodec::TimedCodec(std::unique_ptr<ICompressor> inner)
    : inner_(std::move(inner)) {}

std::vector<uint8_t> TimedCodec::compress(const std::vector<uint8_t> &data) const {
  using namespace std::chrono;
  const auto start = steady_clock::now();
  std::vector<uint8_t> result = inner_->compress(data);
  lastElapsed_ = duration_cast<milliseconds>(steady_clock::now() - start);
  return result;
}

std::vector<uint8_t> TimedCodec::decompress(const std::vector<uint8_t> &data) const {
  using namespace std::chrono;
  const auto start = steady_clock::now();
  std::vector<uint8_t> result = inner_->decompress(data);
  lastElapsed_ = duration_cast<milliseconds>(steady_clock::now() - start);
  return result;
}

// --- ChecksummedCodec ---

ChecksummedCodec::ChecksummedCodec(std::unique_ptr<ICompressor> inner)
    : inner_(std::move(inner)) {}

std::vector<uint8_t> ChecksummedCodec::compress(const std::vector<uint8_t> &data) const {
  std::vector<uint8_t> payload = inner_->compress(data);
  const uint32_t crc = utils::crc32Calculator.calculate(payload.data(),
                                                        payload.size());
  core::MemoryByteSink sink;
  core::BinaryWriter writer(sink);
  writer.bytes(core::ByteView(payload));
  writer.u32(crc);
  return sink.data();
}

std::vector<uint8_t> ChecksummedCodec::decompress(const std::vector<uint8_t> &data) const {
  if (data.size() < 4) {
    throw core::CorruptDataError("Checksummed payload too short");
  }
  const uint32_t expected = static_cast<uint32_t>(data[data.size() - 4]) |
                            (static_cast<uint32_t>(data[data.size() - 3]) << 8) |
                            (static_cast<uint32_t>(data[data.size() - 2]) << 16) |
                            (static_cast<uint32_t>(data[data.size() - 1]) << 24);
  const core::ByteView payload(data.data(), data.size() - 4);
  const uint32_t actual = utils::crc32Calculator.calculate(payload.data(),
                                                           payload.size());
  if (actual != expected) {
    throw core::CorruptDataError("Checksum mismatch in decorated payload");
  }
  return inner_->decompress(
      std::vector<uint8_t>(payload.begin(), payload.end()));
}

// --- ProgressCodec ---

ProgressCodec::ProgressCodec(std::unique_ptr<ICompressor> inner,
                             std::shared_ptr<events::EventBus> events)
    : inner_(std::move(inner)), events_(std::move(events)) {}

void ProgressCodec::publish(events::EventType type, uint64_t in, uint64_t out,
                            uint8_t progressPct) const {
  if (events_) {
    events_->publish(events::CompressionEvent{type,
                                           format::AlgorithmID::UNKNOWN, in,
                                           out, progressPct});
  }
}

std::vector<uint8_t> ProgressCodec::compress(const std::vector<uint8_t> &data) const {
  publish(events::EventType::OperationStarted, data.size(), 0, 0);
  std::vector<uint8_t> result = inner_->compress(data);
  publish(events::EventType::OperationCompleted, data.size(), result.size(), 100);
  return result;
}

std::vector<uint8_t> ProgressCodec::decompress(const std::vector<uint8_t> &data) const {
  publish(events::EventType::OperationStarted, data.size(), 0, 0);
  std::vector<uint8_t> result = inner_->decompress(data);
  publish(events::EventType::OperationCompleted, data.size(), result.size(), 100);
  return result;
}

} // namespace decorator
} // namespace codec
} // namespace compression
