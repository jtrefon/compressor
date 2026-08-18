#include <gtest/gtest.h>
#include <compression/ParallelCompressor.hpp>
#include <compression/NullCompressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <random>
#include <vector>

using namespace compression;

namespace {
std::vector<uint8_t> makeData(size_t n) {
  std::vector<uint8_t> data(n);
  std::mt19937 gen(7);
  for (size_t i = 0; i < n; ++i) data[i] = static_cast<uint8_t>(gen());
  return data;
}
} // namespace

TEST(ParallelCompressorTest, RoundTrip) {
    std::vector<uint8_t> data(1024 * 10, 'A');
    auto base = std::make_unique<NullCompressor>();
    ParallelCompressor pc(std::move(base), format::AlgorithmID::NULL_COMPRESSOR, 2);
    std::vector<uint8_t> compressed = pc.compress(data);
    std::vector<uint8_t> decompressed = pc.decompress(compressed);
    EXPECT_EQ(decompressed, data);
}

TEST(ParallelCompressorTest, MultiChunkThreadVariation) {
  // Chunk boundaries depend on the thread count; exercise sizes that stress
  // the remainder handling (n not divisible by threads) across thread counts.
  for (int threads : {1, 2, 3, 8}) {
    for (size_t n : {1u, 7u, 8u, 9u, 100u, 1000u, 4097u}) {
      auto data = makeData(n);
      ParallelCompressor pc(std::make_unique<NullCompressor>(),
                            format::AlgorithmID::NULL_COMPRESSOR, threads);
      auto compressed = pc.compress(data);
      auto decompressed = pc.decompress(compressed);
      EXPECT_EQ(data, decompressed) << "threads=" << threads << " n=" << n;
    }
  }
}

TEST(ParallelCompressorTest, MultiChunkRealCompressor) {
  auto data = makeData(20000);
  ParallelCompressor pc(std::make_unique<BwtCompressor>(),
                        format::AlgorithmID::BWT_COMPRESSOR, 4);
  auto compressed = pc.compress(data);
  auto decompressed = pc.decompress(compressed);
  EXPECT_EQ(data, decompressed);
}

TEST(ParallelCompressorTest, CorruptedChunkRejected) {
  auto data = makeData(5000);
  ParallelCompressor pc(std::make_unique<NullCompressor>(),
                        format::AlgorithmID::NULL_COMPRESSOR, 4);
  auto compressed = pc.compress(data);
  ASSERT_FALSE(compressed.empty());
  // Flip a payload byte; NullCompressor cannot detect corruption itself, so
  // the checksum verification in ParallelCompressor must reject it.
  compressed[compressed.size() - 1] ^= 0xFF;
  EXPECT_THROW(pc.decompress(compressed), std::runtime_error);
}

TEST(ParallelCompressorTest, InvalidChunkCountRejected) {
  // A crafted header claiming an absurd chunk count must be rejected without
  // spawning threads. Build a minimal header: magic(4) version(1) algo(1)
  // size(8) crc(4) chunkCount(4) chunkSize(4) — chunkCount = 1,000,000.
  std::vector<uint8_t> header;
  const uint8_t magic[4] = {'C', 'P', 'R', 'O'};
  header.insert(header.end(), magic, magic + 4);
  header.push_back(1); // version
  header.push_back(static_cast<uint8_t>(format::AlgorithmID::NULL_COMPRESSOR));
  for (int i = 0; i < 8; ++i) header.push_back(0); // original size
  for (int i = 0; i < 4; ++i) header.push_back(0); // checksum
  const uint32_t big = 1000000;
  for (int i = 0; i < 4; ++i) header.push_back(static_cast<uint8_t>((big >> (i * 8)) & 0xFF));
  for (int i = 0; i < 4; ++i) header.push_back(0); // chunk size

  ParallelCompressor pc(std::make_unique<NullCompressor>(),
                        format::AlgorithmID::NULL_COMPRESSOR, 2);
  EXPECT_THROW(pc.decompress(header), std::runtime_error);
}
