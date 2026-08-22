#include <compression/core/ByteSource.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace compression::core;

namespace {

// RAII temp file with a unique name per test.
class TempFileGuard {
public:
  explicit TempFileGuard(const std::string &name)
      : path_(std::filesystem::temp_directory_path() /
              ("compressor_test_" + name + "_" +
               std::to_string(std::chrono::steady_clock::now()
                                  .time_since_epoch()
                                  .count()))) {}

  ~TempFileGuard() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

std::vector<uint8_t> sampleBytes() {
  std::vector<uint8_t> data(1000);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<uint8_t>((i * 7 + 3) & 0xFF);
  }
  return data;
}

} // namespace

TEST(MemoryByteSourceTest, ReportsSizeAndPosition) {
  const auto data = sampleBytes();
  MemoryByteSource source{ByteView(data)};
  EXPECT_EQ(source.size(), data.size());
  EXPECT_EQ(source.position(), 0u);
  EXPECT_EQ(source.read(nullptr, 0), 0u);
}

TEST(MemoryByteSourceTest, ReadsSequentially) {
  const auto data = sampleBytes();
  MemoryByteSource source{ByteView(data)};
  std::vector<uint8_t> dst(100);
  const std::size_t n = source.read(dst.data(), dst.size());
  EXPECT_EQ(n, 100u);
  EXPECT_EQ(dst, std::vector<uint8_t>(data.begin(), data.begin() + 100));
  EXPECT_EQ(source.position(), 100u);
}

TEST(MemoryByteSourceTest, ReadPastEndReturnsZero) {
  const auto data = sampleBytes();
  MemoryByteSource source{ByteView(data)};
  source.seek(data.size());
  uint8_t byte = 0;
  EXPECT_EQ(source.read(&byte, 1), 0u);
}

TEST(MemoryByteSourceTest, SeekAndReadAtOffset) {
  const auto data = sampleBytes();
  MemoryByteSource source{ByteView(data)};
  source.seek(500);
  std::vector<uint8_t> dst(10);
  EXPECT_EQ(source.read(dst.data(), dst.size()), 10u);
  EXPECT_EQ(dst[0], data[500]);
  EXPECT_EQ(dst[9], data[509]);
}

TEST(MemoryByteSourceTest, SeekBeyondEndThrows) {
  const auto data = sampleBytes();
  MemoryByteSource source{ByteView(data)};
  EXPECT_THROW(source.seek(data.size() + 1), std::out_of_range);
}

TEST(MemoryByteSinkTest, WritesAndReportsSize) {
  MemoryByteSink sink;
  EXPECT_EQ(sink.size(), 0u);
  sink.write(ByteView(std::vector<uint8_t>{1, 2, 3}));
  sink.writeByte(4);
  EXPECT_EQ(sink.size(), 4u);
  const std::vector<uint8_t> expected = {1, 2, 3, 4};
  EXPECT_EQ(sink.data(), expected);
}

TEST(MemoryByteSinkTest, TakeMovesData) {
  MemoryByteSink sink;
  sink.write(ByteView(std::vector<uint8_t>{9}));
  auto data = sink.take();
  EXPECT_EQ(data.size(), 1u);
  EXPECT_EQ(sink.size(), 0u);
}

TEST(FileByteSourceTest, FileRoundTrip) {
  TempFileGuard file("roundtrip");
  {
    FileByteSink sink(file.path());
    sink.write(ByteView(sampleBytes()));
    EXPECT_EQ(sink.size(), 1000u);
  }

  FileByteSource source(file.path());
  EXPECT_EQ(source.size(), 1000u);
  EXPECT_EQ(source.position(), 0u);

  std::vector<uint8_t> read(1000);
  std::size_t total = 0;
  while (total < read.size()) {
    const std::size_t n = source.read(read.data() + total, read.size() - total);
    ASSERT_GT(n, 0u);
    total += n;
  }
  EXPECT_EQ(read, sampleBytes());
  EXPECT_EQ(source.read(read.data(), 1), 0u);
}

TEST(FileByteSourceTest, MissingFileThrowsIoError) {
  TempFileGuard file("missing");
  EXPECT_THROW(FileByteSource source(file.path()), IoError);
}

TEST(FileByteSourceTest, SeekThenReadAtOffset) {
  TempFileGuard file("seek");
  {
    FileByteSink sink(file.path());
    sink.write(ByteView(sampleBytes()));
  }

  FileByteSource source(file.path());
  source.seek(500);
  EXPECT_EQ(source.position(), 500u);
  uint8_t byte = 0;
  EXPECT_EQ(source.read(&byte, 1), 1u);
  EXPECT_EQ(byte, sampleBytes()[500]);
}

TEST(FileByteSinkTest, OverwritesExistingFile) {
  TempFileGuard file("overwrite");
  {
    FileByteSink sink(file.path());
    sink.write(ByteView(sampleBytes()));
  }
  {
    FileByteSink sink(file.path());
    sink.write(ByteView(std::vector<uint8_t>{42}));
  }
  FileByteSource source(file.path());
  EXPECT_EQ(source.size(), 1u);
  uint8_t byte = 0;
  EXPECT_EQ(source.read(&byte, 1), 1u);
  EXPECT_EQ(byte, 42u);
}
