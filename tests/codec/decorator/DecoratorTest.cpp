#include <compression/codec/decorator/Decorators.hpp>

#include <compression/NullCompressor.hpp>
#include <compression/RleCompressor.hpp>
#include <compression/app/EventBus.hpp>
#include <compression/core/Errors.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace compression;
using namespace compression::codec::decorator;

namespace {

std::vector<uint8_t> textData(size_t n) {
  std::vector<uint8_t> data;
  const std::string words[] = {"decorator", "composition", "pattern"};
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

TEST(TimedCodecTest, MeasuresElapsed) {
  TimedCodec codec(std::make_unique<NullCompressor>());
  const auto data = textData(1000);
  const auto compressed = codec.compress(data);
  EXPECT_EQ(compressed, data);
  EXPECT_GE(codec.lastElapsed().count(), 0);
}

TEST(ChecksummedCodecTest, RoundTripAndVerification) {
  ChecksummedCodec codec(std::make_unique<RleCompressor>());
  const auto data = textData(5000);
  const auto compressed = codec.compress(data);
  EXPECT_GT(compressed.size(), data.size()); // RLE grows non-repetitive data
  EXPECT_EQ(codec.decompress(compressed), data);
}

TEST(ChecksummedCodecTest, CorruptPayloadRejected) {
  ChecksummedCodec codec(std::make_unique<RleCompressor>());
  auto compressed = codec.compress(textData(5000));
  compressed[compressed.size() / 2] ^= 0xFF;
  EXPECT_THROW(codec.decompress(compressed), core::CorruptDataError);
}

TEST(ChecksummedCodecTest, TooShortRejected) {
  ChecksummedCodec codec(std::make_unique<RleCompressor>());
  const std::vector<uint8_t> junk = {0x01, 0x02};
  EXPECT_THROW(codec.decompress(junk), core::CorruptDataError);
}

TEST(ProgressCodecTest, PublishesEvents) {
  auto bus = std::make_shared<app::EventBus>();
  class Listener final : public app::IEventListener {
  public:
    void onEvent(const app::CompressionEvent &event) override {
      ++count;
    }
    int count = 0;
  };
  auto listener = std::make_shared<Listener>();
  bus->subscribe(listener);

  ProgressCodec codec(std::make_unique<NullCompressor>(), bus);
  const auto data = textData(100);
  (void)codec.compress(data);
  EXPECT_GE(listener->count, 1);
}

TEST(DecoratorCompositionTest, ChecksumThenProgress) {
  auto bus = std::make_shared<app::EventBus>();
  class Listener final : public app::IEventListener {
  public:
    void onEvent(const app::CompressionEvent &event) override { ++count; }
    int count = 0;
  };
  auto listener = std::make_shared<Listener>();
  bus->subscribe(listener);

  ProgressCodec progress(std::make_unique<ChecksummedCodec>(
                             std::make_unique<RleCompressor>()),
                         bus);
  const auto data = textData(2000);
  const auto compressed = progress.compress(data);
  EXPECT_EQ(progress.decompress(compressed), data);
  EXPECT_GE(listener->count, 1);
}
