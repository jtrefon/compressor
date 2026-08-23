#include <compression/codec/ParallelCodecDecorator.hpp>

#include <compression/codec/legacy/BwtCompressor.hpp>
#include <compression/codec/legacy/NullCompressor.hpp>
#include <compression/ThreadPool.hpp>
#include <compression/events/EventBus.hpp>
#include <compression/core/Errors.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace compression;
using namespace compression::codec;

namespace {

std::vector<uint8_t> textData(size_t n) {
  std::vector<uint8_t> data;
  data.reserve(n);
  const std::string words[] = {"parallel", "codec", "decorator"};
  size_t w = 0;
  while (data.size() < n) {
    for (char c : words[w % 3]) {
      data.push_back(static_cast<uint8_t>(c));
    }
    data.push_back(' ');
    w++;
  }
  data.resize(n);
  return data;
}

} // namespace

TEST(ParallelCodecDecoratorTest, IsAnICompressor) {
  EXPECT_TRUE((std::is_base_of<ICompressor, ParallelCodecDecorator>::value));
}

TEST(ParallelCodecDecoratorTest, SingleChunkRoundTrip) {
  core::InlineExecutor executor;
  ParallelCodecDecorator codec(std::make_unique<NullCompressor>(),
                               format::AlgorithmID::NULL_COMPRESSOR, &executor,
                               1);
  const auto data = textData(1000);
  EXPECT_EQ(codec.decompress(codec.compress(data)), data);
}

TEST(ParallelCodecDecoratorTest, InlineExecutorMultiChunkRoundTrip) {
  // Deterministic: the InlineExecutor runs chunks on the calling thread.
  core::InlineExecutor executor;
  ParallelCodecDecorator codec(std::make_unique<NullCompressor>(),
                               format::AlgorithmID::NULL_COMPRESSOR, &executor,
                               4);
  const auto data = textData(5000);
  EXPECT_EQ(codec.decompress(codec.compress(data)), data);
}

TEST(ParallelCodecDecoratorTest, ThreadPoolMultiChunkRoundTrip) {
  ThreadPool pool(4);
  ParallelCodecDecorator codec(std::make_unique<BwtCompressor>(),
                               format::AlgorithmID::BWT_COMPRESSOR, &pool, 4);
  const auto data = textData(30000);
  EXPECT_EQ(codec.decompress(codec.compress(data)), data);
}

TEST(ParallelCodecDecoratorTest, AutoChunkCount) {
  ThreadPool pool(4);
  ParallelCodecDecorator codec(std::make_unique<NullCompressor>(),
                               format::AlgorithmID::NULL_COMPRESSOR, &pool, 0);
  const auto data = textData(20000);
  EXPECT_EQ(codec.decompress(codec.compress(data)), data);
}

TEST(ParallelCodecDecoratorTest, EmptyData) {
  core::InlineExecutor executor;
  ParallelCodecDecorator codec(std::make_unique<NullCompressor>(),
                               format::AlgorithmID::NULL_COMPRESSOR, &executor,
                               4);
  const auto compressed = codec.compress({});
  EXPECT_EQ(codec.decompress(compressed), std::vector<uint8_t>{});
}

TEST(ParallelCodecDecoratorTest, PublishesPerChunkEvents) {
  auto bus = std::make_shared<events::EventBus>();
  class Listener final : public events::IEventListener {
  public:
    void onEvent(const events::CompressionEvent &event) override {
      if (event.type == events::EventType::ChunkProgress) {
        ++chunkEvents;
      }
    }
    int chunkEvents = 0;
  };
  auto listener = std::make_shared<Listener>();
  bus->subscribe(listener);

  ThreadPool pool(4);
  ParallelCodecDecorator codec(std::make_unique<NullCompressor>(),
                               format::AlgorithmID::NULL_COMPRESSOR, &pool, 4,
                               bus);
  const auto data = textData(10000);
  const auto compressed = codec.compress(data);
  EXPECT_GE(listener->chunkEvents, 1);
  EXPECT_EQ(codec.decompress(compressed), data);
}

TEST(ParallelCodecDecoratorTest, CorruptedChunkRejected) {
  core::InlineExecutor executor;
  ParallelCodecDecorator codec(std::make_unique<NullCompressor>(),
                               format::AlgorithmID::NULL_COMPRESSOR, &executor,
                               4);
  auto compressed = codec.compress(textData(8000));
  compressed[compressed.size() / 2] ^= 0xFF;
  EXPECT_THROW(codec.decompress(compressed), core::CorruptDataError);
}

TEST(ParallelCodecDecoratorTest, InvalidChunkCountRejected) {
  // Crafted header claiming an absurd chunk count must be rejected without
  // spawning work: magic(4) version(1) algo(1) size(8) crc(4) count(4)
  // chunkSize(4).
  core::InlineExecutor executor;
  ParallelCodecDecorator codec(std::make_unique<NullCompressor>(),
                               format::AlgorithmID::NULL_COMPRESSOR, &executor,
                               2);
  std::vector<uint8_t> header = {'C', 'P', 'R', 'O',
                                 1,
                                 static_cast<uint8_t>(format::AlgorithmID::NULL_COMPRESSOR),
                                 0, 0, 0, 0, 0, 0, 0, 0,
                                 0, 0, 0, 0,
                                 0, 0, 0, 0,
                                 0, 0, 0, 0};
  const uint32_t big = 1000000;
  for (int i = 0; i < 4; ++i) {
    header.push_back(static_cast<uint8_t>((big >> (i * 8)) & 0xFF));
  }
  header.insert(header.end(), 4, 0);
  EXPECT_THROW(codec.decompress(header), core::CorruptDataError);
}
