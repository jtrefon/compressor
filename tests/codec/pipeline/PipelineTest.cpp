#include <compression/codec/pipeline/Pipeline.hpp>

#include <compression/FileFormat.hpp>
#include <compression/codec/CodecRegistry.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace compression;
using namespace compression::codec::pipeline;

namespace {

std::vector<uint8_t> textData(size_t n) {
  std::vector<uint8_t> data;
  data.reserve(n);
  const std::string words[] = {"alpha", "bravo", "charlie", "delta"};
  size_t w = 0;
  while (data.size() < n) {
    for (char c : words[w % 4]) {
      data.push_back(static_cast<uint8_t>(c));
    }
    data.push_back(' ');
    w++;
  }
  data.resize(n);
  return data;
}

std::vector<uint8_t> runnyData(size_t n) {
  std::vector<uint8_t> data;
  data.reserve(n);
  size_t written = 0;
  uint8_t v = 0;
  while (written < n) {
    const size_t run = std::min<size_t>(n - written, (v % 7) + 3);
    data.insert(data.end(), run, static_cast<uint8_t>(v++));
    written += run;
  }
  return data;
}

} // namespace

TEST(RleTransformTest, RoundTripVarious) {
  RleTransform rle;
  const std::vector<std::vector<uint8_t>> cases = {
      {},
      {0x41},
      {0x41, 0x42, 0x43},
      std::vector<uint8_t>(100, 0x00),
      std::vector<uint8_t>(300, 0xFF), // spans multiple tokens (>255 run)
      runnyData(5000),
      textData(2000),
      {0x00, 0x00, 0x00, 0x41, 0x41, 0xFF},
  };
  for (const auto &data : cases) {
    EXPECT_EQ(rle.inverse(core::ByteView(rle.forward(core::ByteView(data)))),
              data);
  }
}

TEST(RleTransformTest, CompressesRuns) {
  RleTransform rle;
  const std::vector<uint8_t> data(200, 0x07);
  const auto encoded = rle.forward(core::ByteView(data));
  EXPECT_LT(encoded.size(), data.size());
}

TEST(RleTransformTest, TruncatedTokenThrows) {
  RleTransform rle;
  EXPECT_THROW(rle.inverse(core::ByteView(std::vector<uint8_t>{0x00, 0x05})),
               core::CorruptDataError);
  EXPECT_THROW(rle.inverse(core::ByteView(std::vector<uint8_t>{0x00, 0x05, 0x01, 0x02})),
               core::CorruptDataError);
  EXPECT_THROW(rle.inverse(core::ByteView(std::vector<uint8_t>{0x7F})),
               core::CorruptDataError);
}

TEST(BwtTransformAdapterTest, RoundTrip) {
  BwtTransformAdapter bwt;
  const auto data = textData(3000);
  EXPECT_EQ(bwt.inverse(core::ByteView(bwt.forward(core::ByteView(data)))),
            data);
}

TEST(PipelineCodecTest, RoundTrip) {
  std::vector<std::unique_ptr<ITransform>> transforms;
  transforms.push_back(std::make_unique<BwtTransformAdapter>());
  transforms.push_back(std::make_unique<RleTransform>());
  PipelineCodec codec(std::move(transforms),
                      std::make_unique<ArithmeticCoderAdapter>());

  const auto data = textData(20000);
  const auto compressed = codec.compress(data);
  EXPECT_LT(compressed.size(), data.size());
  EXPECT_EQ(codec.decompress(compressed), data);
}

TEST(PipelineCodecTest, EmptyRoundTrip) {
  std::vector<std::unique_ptr<ITransform>> transforms;
  transforms.push_back(std::make_unique<BwtTransformAdapter>());
  PipelineCodec codec(std::move(transforms),
                      std::make_unique<ArithmeticCoderAdapter>());
  EXPECT_TRUE(codec.decompress(codec.compress({})).empty());
}

TEST(PipelineCodecTest, SelfDescribingFrame) {
  // The emitted bytes start with the pipeline magic.
  std::vector<std::unique_ptr<ITransform>> transforms;
  transforms.push_back(std::make_unique<RleTransform>());
  PipelineCodec codec(std::move(transforms),
                      std::make_unique<ArithmeticCoderAdapter>());
  const auto compressed = codec.compress(textData(100));
  const char magic[4] = {'P', 'L', 'I', 'N'};
  ASSERT_GE(compressed.size(), 4u);
  EXPECT_TRUE(std::equal(magic, magic + 4, compressed.begin()));
}

TEST(PipelineCodecTest, CorruptFrameThrows) {
  std::vector<std::unique_ptr<ITransform>> transforms;
  transforms.push_back(std::make_unique<RleTransform>());
  PipelineCodec codec(std::move(transforms),
                      std::make_unique<ArithmeticCoderAdapter>());
  auto compressed = codec.compress(textData(500));
  compressed[compressed.size() / 2] ^= 0xFF;
  EXPECT_THROW(codec.decompress(compressed), std::exception);
}

TEST(PipelineCodecTest, UnknownStageIdRejected) {
  std::vector<std::unique_ptr<ITransform>> transforms;
  transforms.push_back(std::make_unique<RleTransform>());
  PipelineCodec codec(std::move(transforms),
                      std::make_unique<ArithmeticCoderAdapter>());
  auto compressed = codec.compress(textData(50));
  // Corrupt the stage id (at offset 6: magic 4 + version 1 + count 1).
  compressed[6] = 0x7F;
  EXPECT_THROW(codec.decompress(compressed), core::ConfigurationError);
}

TEST(CodecRegistryBwt2Test, RegisteredAndRoundTrips) {
  const auto &registry = codec::CodecRegistry::instance();
  EXPECT_TRUE(registry.contains("bwt2"));
  auto codec = registry.create("bwt2");
  const auto data = textData(10000);
  EXPECT_EQ(codec->decompress(codec->compress(data)), data);
}

TEST(CodecRegistryBwt2Test, IdMapping) {
  const auto &registry = codec::CodecRegistry::instance();
  EXPECT_EQ(registry.idOf("bwt2"),
            format::AlgorithmID::PIPELINE_BWT_COMPRESSOR);
  EXPECT_EQ(registry.nameOf(format::AlgorithmID::PIPELINE_BWT_COMPRESSOR),
            "bwt2");
}
