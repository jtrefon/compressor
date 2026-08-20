#include <compression/app/CompressionService.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace compression;

namespace {

// RAII temp files with unique names.
class TempPath {
public:
  explicit TempPath(const std::string &name)
      : path_(std::filesystem::temp_directory_path() /
              ("compressor_m2_" + name + "_" +
               std::to_string(std::chrono::steady_clock::now()
                                  .time_since_epoch()
                                  .count()))) {}

  ~TempPath() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

std::vector<uint8_t> textData(size_t n) {
  std::vector<uint8_t> data;
  data.reserve(n);
  const std::string words[] = {"the",   "quick", "brown", "fox", "jumps",
                               "over",  "lazy",  "dog",   "compression",
                               "test"};
  size_t w = 0;
  while (data.size() < n) {
    for (char c : words[w % 10]) {
      data.push_back(static_cast<uint8_t>(c));
    }
    data.push_back(' ');
    w++;
  }
  data.resize(n);
  return data;
}

} // namespace

TEST(CompressionServiceTest, InMemoryRoundTripDefault) {
  CompressionService service;
  const auto data = textData(30000);

  core::MemoryByteSink compressed;
  CompressResult cr = service.compress(core::ByteView(data), compressed);
  EXPECT_EQ(cr.inBytes, data.size());
  EXPECT_GT(cr.outBytes, 0u);
  EXPECT_GT(cr.crc, 0u);

  core::MemoryByteSink decompressed;
  ExtractResult dr = service.decompress(core::ByteView(compressed.data()),
                                        decompressed);
  EXPECT_EQ(dr.outBytes, data.size());
  EXPECT_TRUE(dr.verified);
  EXPECT_EQ(decompressed.data(), data);
}

TEST(CompressionServiceTest, InMemoryRoundTripBwt) {
  CompressionService service;
  const auto data = textData(20000);
  CompressionOptions options;
  options.codec = format::AlgorithmID::BWT_COMPRESSOR;

  core::MemoryByteSink compressed;
  service.compress(core::ByteView(data), compressed, options);
  core::MemoryByteSink decompressed;
  service.decompress(core::ByteView(compressed.data()), decompressed);
  EXPECT_EQ(decompressed.data(), data);
}

TEST(CompressionServiceTest, EmptyInputRoundTrips) {
  CompressionService service;
  const std::vector<uint8_t> data;

  core::MemoryByteSink compressed;
  CompressResult cr = service.compress(core::ByteView(data), compressed);
  EXPECT_EQ(cr.inBytes, 0u);
  EXPECT_EQ(cr.ratio, 0.0);

  core::MemoryByteSink decompressed;
  service.decompress(core::ByteView(compressed.data()), decompressed);
  EXPECT_TRUE(decompressed.data().empty());
}

TEST(CompressionServiceTest, MultiThreadedRoundTrip) {
  CompressionService service;
  const auto data = textData(200000);
  CompressionOptions options;
  options.codec = format::AlgorithmID::OPTIMIZED_COMPRESSOR;
  options.threads = 4;

  core::MemoryByteSink compressed;
  service.compress(core::ByteView(data), compressed, options);
  core::MemoryByteSink decompressed;
  service.decompress(core::ByteView(compressed.data()), decompressed);
  EXPECT_EQ(decompressed.data(), data);
}

TEST(CompressionServiceTest, AutoThreadsRoundTrip) {
  CompressionService service;
  const auto data = textData(100000);
  CompressionOptions options;
  options.threads = 0; // auto

  core::MemoryByteSink compressed;
  service.compress(core::ByteView(data), compressed, options);
  core::MemoryByteSink decompressed;
  service.decompress(core::ByteView(compressed.data()), decompressed);
  EXPECT_EQ(decompressed.data(), data);
}

TEST(CompressionServiceTest, UnknownCodecThrowsConfigurationError) {
  CompressionService service;
  CompressionOptions options;
  options.codec = format::AlgorithmID::UNKNOWN;
  core::MemoryByteSink sink;
  EXPECT_THROW(service.compress(core::ByteView(textData(10)), sink, options),
               core::ConfigurationError);
}

TEST(CompressionServiceTest, CorruptPayloadRejected) {
  CompressionService service;
  const auto data = textData(20000);

  core::MemoryByteSink compressed;
  service.compress(core::ByteView(data), compressed);
  ASSERT_GT(compressed.data().size(), 0u);
  // Flip a payload byte (after the header).
  std::vector<uint8_t> corrupted = compressed.data();
  corrupted[corrupted.size() / 2] ^= 0xFF;

  core::MemoryByteSink sink;
  EXPECT_THROW(service.decompress(core::ByteView(corrupted), sink),
               std::runtime_error);
}

TEST(CompressionServiceTest, FileRoundTrip) {
  CompressionService service;
  TempPath in("in");
  TempPath out("out");
  TempPath back("back");
  {
    core::FileByteSink sink(in.path());
    sink.write(core::ByteView(textData(50000)));
  }

  CompressResult cr = service.compressFile(in.path(), out.path());
  EXPECT_EQ(cr.inBytes, 50000u);

  ExtractResult dr = service.decompressFile(out.path(), back.path());
  EXPECT_EQ(dr.outBytes, 50000u);

  core::FileByteSource source(back.path());
  std::vector<uint8_t> result;
  std::vector<uint8_t> buf(4096);
  while (true) {
    const std::size_t n = source.read(buf.data(), buf.size());
    if (n == 0) {
      break;
    }
    result.insert(result.end(), buf.begin(), buf.begin() + n);
  }
  EXPECT_EQ(result, textData(50000));
}

TEST(CompressionServiceTest, PublishesOperationEvents) {
  auto bus = std::make_shared<app::EventBus>();
  class Listener final : public app::IEventListener {
  public:
    void onEvent(const app::CompressionEvent &event) override {
      types_.push_back(event.type);
    }
    std::vector<app::EventType> types_;
  };
  auto listener = std::make_shared<Listener>();
  bus->subscribe(listener);

  CompressionService service(bus);
  const auto data = textData(1000);
  core::MemoryByteSink compressed;
  service.compress(core::ByteView(data), compressed);

  bool sawStarted = false;
  bool sawCompleted = false;
  for (auto t : listener->types_) {
    sawStarted = sawStarted || t == app::EventType::OperationStarted;
    sawCompleted = sawCompleted || t == app::EventType::OperationCompleted;
  }
  EXPECT_TRUE(sawStarted);
  EXPECT_TRUE(sawCompleted);
}
