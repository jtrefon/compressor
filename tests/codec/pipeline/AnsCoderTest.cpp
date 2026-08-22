#include <compression/codec/pipeline/AnsCoder.hpp>
#include <compression/codec/pipeline/Pipeline.hpp>

#include <compression/core/Errors.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <string>
#include <vector>

using namespace compression;
using namespace compression::codec;
using namespace compression::codec::pipeline;

namespace {

std::vector<uint8_t> textData(size_t n) {
  std::vector<uint8_t> data;
  const std::string text =
      "The quick brown fox jumps over the lazy dog. rANS pays for itself "
      "when a model concentrates probability mass on few symbols. ";
  while (data.size() < n) {
    for (char c : text) {
      data.push_back(static_cast<uint8_t>(c));
      if (data.size() == n) {
        break;
      }
    }
  }
  return data;
}

std::vector<uint8_t> randomData(size_t n, uint32_t seed) {
  std::mt19937 rng(seed);
  std::vector<uint8_t> data(n);
  for (uint8_t &b : data) {
    b = static_cast<uint8_t>(rng() & 0xFF);
  }
  return data;
}

} // namespace

TEST(AnsCoderTest, ReportsStableId) {
  EXPECT_EQ(AnsCoder().id(), kCoderAns);
}

TEST(AnsCoderTest, RoundTripText) {
  const AnsCoder coder;
  const auto data = textData(5000);
  EXPECT_EQ(coder.decode(coder.encode(data)), data);
}

TEST(AnsCoderTest, RoundTripRandom) {
  const AnsCoder coder;
  const auto data = randomData(5000, 42);
  EXPECT_EQ(coder.decode(coder.encode(data)), data);
}

TEST(AnsCoderTest, RoundTripZeros) {
  const AnsCoder coder;
  const std::vector<uint8_t> data(5000, 0x00);
  EXPECT_EQ(coder.decode(coder.encode(data)), data);
}

TEST(AnsCoderTest, RoundTripSingleSymbol) {
  const AnsCoder coder;
  const std::vector<uint8_t> data(5000, 0xAB);
  EXPECT_EQ(coder.decode(coder.encode(data)), data);
}

TEST(AnsCoderTest, RoundTripAllSymbols) {
  const AnsCoder coder;
  std::vector<uint8_t> data;
  for (int i = 0; i < 256; ++i) {
    data.insert(data.end(), 8, static_cast<uint8_t>(i));
  }
  EXPECT_EQ(coder.decode(coder.encode(data)), data);
}

TEST(AnsCoderTest, RoundTripEmpty) {
  const AnsCoder coder;
  const std::vector<uint8_t> encoded = coder.encode({});
  EXPECT_EQ(coder.decode(encoded), std::vector<uint8_t>{});
}

TEST(AnsCoderTest, RoundTripLarge) {
  const AnsCoder coder;
  const auto data = textData(200000);
  EXPECT_EQ(coder.decode(coder.encode(data)), data);
}

TEST(AnsCoderTest, EncodeIsDeterministic) {
  const AnsCoder coder;
  const auto data = textData(1000);
  EXPECT_EQ(coder.encode(data), coder.encode(data));
}

TEST(AnsCoderTest, CompressesRepetitiveData) {
  const AnsCoder coder;
  const std::vector<uint8_t> data(20000, 'a');
  const auto encoded = coder.encode(data);
  EXPECT_LT(encoded.size(), data.size() / 2);
}

TEST(AnsCoderTest, WorksAsPipelineStage) {
  // "ans" = PipelineCodec with no transforms + the ANS coder.
  auto codec = std::make_unique<PipelineCodec>(
      std::vector<std::unique_ptr<ITransform>>{}, std::make_unique<AnsCoder>());
  const auto data = textData(10000);
  EXPECT_EQ(codec->decompress(codec->compress(data)), data);
}

TEST(AnsCoderTest, RejectsTruncatedPayload) {
  const AnsCoder coder;
  const auto data = textData(200);
  const auto encoded = coder.encode(data);
  for (size_t cut = 0; cut < encoded.size(); cut += 16) {
    EXPECT_THROW(coder.decode(std::vector<uint8_t>(encoded.begin(),
                                                    encoded.begin() + cut)),
                 core::CorruptDataError);
  }
}

TEST(AnsCoderTest, RejectsBadFrequencySum) {
  const AnsCoder coder;
  const auto data = textData(200);
  auto encoded = coder.encode(data);
  // Corrupt a low byte of a frequency entry.
  encoded[0] ^= 0x01;
  EXPECT_THROW(coder.decode(encoded), core::CorruptDataError);
}

TEST(AnsCoderTest, RejectsHugeSymbolCount) {
  const AnsCoder coder;
  const auto data = textData(200);
  auto encoded = coder.encode(data);
  // Force the u32 symbol count to a value larger than the payload.
  encoded[512] = 0xFF;
  encoded[513] = 0xFF;
  encoded[514] = 0xFF;
  encoded[515] = 0xFF;
  EXPECT_THROW(coder.decode(encoded), core::CorruptDataError);
}

TEST(AnsCoderTest, RejectsTrailingBytes) {
  const AnsCoder coder;
  const auto data = textData(200);
  auto encoded = coder.encode(data);
  encoded.push_back(0x00);
  EXPECT_THROW(coder.decode(encoded), core::CorruptDataError);
}

TEST(AnsCoderTest, ByteFlipsNeverCrash) {
  const AnsCoder coder;
  const auto data = textData(100);
  const auto encoded = coder.encode(data);
  for (size_t i = 0; i < encoded.size(); ++i) {
    auto mutated = encoded;
    mutated[i] ^= 0x40;
    try {
      (void)coder.decode(mutated);
    } catch (const core::CorruptDataError &) {
      // Expected: corrupted input is rejected.
    }
  }
}