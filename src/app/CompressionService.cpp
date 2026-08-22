#include <compression/app/CompressionService.hpp>

#include <compression/Crc32.hpp>
#include <compression/SystemInfo.hpp>
#include <compression/ThreadPool.hpp>
#include <compression/codec/CodecRegistry.hpp>
#include <compression/codec/ParallelCodecDecorator.hpp>
#include <compression/core/Errors.hpp>

#include <chrono>
#include <utility>
#include <vector>

namespace compression {

namespace {

std::vector<uint8_t> toVector(core::ByteView view) {
  return std::vector<uint8_t>(view.begin(), view.end());
}

std::vector<uint8_t> readAll(core::IByteSource &source) {
  std::vector<uint8_t> out;
  out.reserve(static_cast<std::size_t>(source.size()));
  std::vector<uint8_t> buffer(64 * 1024);
  while (true) {
    const std::size_t n = source.read(buffer.data(), buffer.size());
    if (n == 0) {
      break;
    }
    out.insert(out.end(), buffer.begin(), buffer.begin() + n);
  }
  return out;
}

} // namespace

CompressionService::CompressionService(std::shared_ptr<app::EventBus> events)
    : events_(std::move(events)) {}

void CompressionService::publish(app::EventType type, format::AlgorithmID codec,
                                 uint64_t in, uint64_t out,
                                 uint8_t progressPct) const {
  if (events_) {
    events_->publish(app::CompressionEvent{type, codec, in, out, progressPct});
  }
}

CompressResult CompressionService::compress(core::ByteView data,
                                            core::IByteSink &out,
                                            const CompressionOptions &options) {
  using namespace std::chrono;
  const auto start = steady_clock::now();

  const uint32_t crc = utils::crc32Calculator.calculate(data.data(), data.size());
  publish(app::EventType::OperationStarted, options.codec, data.size(), 0, 0);
  publish(app::EventType::CodecSelected, options.codec, data.size(), 0, 0);

  auto codec = codec::CodecRegistry::instance().create(options.codec);
  const std::size_t threads =
      options.threads == 0 ? getHardwareThreads() : options.threads;

  std::vector<uint8_t> framed;
  if (threads <= 1) {
    codec::ParallelCodecDecorator parallel(std::move(codec), options.codec,
                                           nullptr, 1, events_);
    framed = parallel.compress(toVector(data));
  } else {
    ThreadPool pool(threads);
    codec::ParallelCodecDecorator parallel(std::move(codec), options.codec,
                                           &pool, threads, events_);
    framed = parallel.compress(toVector(data));
  }

  out.write(core::ByteView(framed));
  publish(app::EventType::OperationCompleted, options.codec, data.size(),
          framed.size(), 100);

  CompressResult result;
  result.inBytes = data.size();
  result.outBytes = framed.size();
  result.ratio = data.empty() ? 0.0
                              : static_cast<double>(framed.size()) /
                                    static_cast<double>(data.size());
  result.crc = crc;
  result.elapsed =
      duration_cast<milliseconds>(steady_clock::now() - start);
  return result;
}

ExtractResult CompressionService::decompress(core::ByteView data,
                                             core::IByteSink &out) {
  using namespace std::chrono;
  const auto start = steady_clock::now();

  // The container header identifies the codec; a placeholder base is fine
  // because decompression rebuilds codecs per chunk from the header.
  auto placeholder =
      codec::CodecRegistry::instance().create(format::AlgorithmID::NULL_COMPRESSOR);
  ThreadPool pool(getHardwareThreads());
  codec::ParallelCodecDecorator parallel(std::move(placeholder),
                                         format::AlgorithmID::UNKNOWN, &pool, 0,
                                         events_);
  std::vector<uint8_t> original = parallel.decompress(toVector(data));

  out.write(core::ByteView(original));
  const uint32_t crc = utils::crc32Calculator.calculate(original.data(),
                                                        original.size());
  publish(app::EventType::OperationCompleted, format::AlgorithmID::UNKNOWN,
          data.size(), original.size(), 100);

  ExtractResult result;
  result.inBytes = data.size();
  result.outBytes = original.size();
  result.crc = crc;
  result.verified = true; // the parallel codec verifies CRC and throws on mismatch
  result.elapsed =
      duration_cast<milliseconds>(steady_clock::now() - start);
  return result;
}

CompressResult CompressionService::compressFile(
    const std::filesystem::path &in, const std::filesystem::path &out,
    const CompressionOptions &options) {
  core::FileByteSource source(in);
  std::vector<uint8_t> data = readAll(source);
  core::FileByteSink sink(out);
  return compress(core::ByteView(data), sink, options);
}

ExtractResult CompressionService::decompressFile(
    const std::filesystem::path &in, const std::filesystem::path &out) {
  core::FileByteSource source(in);
  std::vector<uint8_t> data = readAll(source);
  core::FileByteSink sink(out);
  return decompress(core::ByteView(data), sink);
}

} // namespace compression
