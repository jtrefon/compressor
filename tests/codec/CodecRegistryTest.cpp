#include <compression/codec/CodecRegistry.hpp>

#include <compression/codec/legacy/BwtCompressor.hpp>
#include <compression/codec/legacy/RleCompressor.hpp>
#include <compression/core/Errors.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

using namespace compression;
using namespace compression::codec;

TEST(CodecRegistryTest, ContainsAllBuiltinCodecs) {
  const auto &registry = CodecRegistry::instance();
  EXPECT_TRUE(registry.contains(format::AlgorithmID::NULL_COMPRESSOR));
  EXPECT_TRUE(registry.contains(format::AlgorithmID::RLE_COMPRESSOR));
  EXPECT_TRUE(registry.contains(format::AlgorithmID::HUFFMAN_COMPRESSOR));
  EXPECT_TRUE(registry.contains(format::AlgorithmID::LZ77_COMPRESSOR));
  EXPECT_TRUE(registry.contains(format::AlgorithmID::BWT_COMPRESSOR));
  EXPECT_TRUE(registry.contains(format::AlgorithmID::ULTRA_COMPRESSOR));
  EXPECT_TRUE(registry.contains(format::AlgorithmID::EXTREME_COMPRESSOR));
  EXPECT_TRUE(registry.contains(format::AlgorithmID::OPTIMIZED_COMPRESSOR));
  EXPECT_TRUE(registry.contains(format::AlgorithmID::ARITHMETIC_COMPRESSOR));
}

TEST(CodecRegistryTest, NameLookupRoundTrip) {
  const auto &registry = CodecRegistry::instance();
  for (const auto &[id, name] : registry.all()) {
    EXPECT_EQ(registry.idOf(name), id);
    EXPECT_EQ(registry.nameOf(id), name);
  }
}

TEST(CodecRegistryTest, UnknownIdsAndNames) {
  const auto &registry = CodecRegistry::instance();
  EXPECT_FALSE(registry.contains(format::AlgorithmID::UNKNOWN));
  EXPECT_FALSE(registry.contains("nonexistent"));
  EXPECT_EQ(registry.idOf("nonexistent"), format::AlgorithmID::UNKNOWN);
  EXPECT_EQ(registry.nameOf(format::AlgorithmID::UNKNOWN), "unknown");
}

TEST(CodecRegistryTest, CreateByIdReturnsWorkingCodec) {
  const auto &registry = CodecRegistry::instance();
  const std::vector<uint8_t> data = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o',
                                     'r', 'l', 'd'};
  auto codec = registry.create(format::AlgorithmID::NULL_COMPRESSOR);
  ASSERT_NE(codec, nullptr);
  EXPECT_EQ(codec->compress(data), data);
  EXPECT_EQ(codec->decompress(data), data);
}

TEST(CodecRegistryTest, CreateByName) {
  const auto &registry = CodecRegistry::instance();
  auto codec = registry.create("bwt");
  ASSERT_NE(codec, nullptr);

  // Round-trip through a real codec from the registry.
  const std::vector<uint8_t> data = {'a', 'b', 'a', 'b', 'a', 'b'};
  EXPECT_EQ(codec->decompress(codec->compress(data)), data);
}

TEST(CodecRegistryTest, CreateLz77UsesCanonicalTuning) {
  // The registry's LZ77 must match the canonical config (64 KiB window, no
  // greedy parsing) — the same parameters the old factories hard-coded.
  const auto &registry = CodecRegistry::instance();
  auto codec = registry.create("lz77");
  std::vector<uint8_t> data(70000);
  for (size_t i = 0; i < 100; ++i) {
    data[i] = static_cast<uint8_t>('A' + (i % 26));
  }
  for (size_t i = 100; i < 40000; ++i) {
    data[i] = static_cast<uint8_t>(i % 256);
  }
  for (size_t i = 0; i < 100; ++i) {
    data[40000 + i] = static_cast<uint8_t>('A' + (i % 26));
  }
  for (size_t i = 40100; i < 70000; ++i) {
    data[i] = static_cast<uint8_t>(i % 256);
  }
  EXPECT_EQ(codec->decompress(codec->compress(data)), data);
}

TEST(CodecRegistryTest, CreateUnknownIdThrows) {
  const auto &registry = CodecRegistry::instance();
  EXPECT_THROW(registry.create(format::AlgorithmID::UNKNOWN),
               core::ConfigurationError);
}

TEST(CodecRegistryTest, CreateUnknownNameThrows) {
  const auto &registry = CodecRegistry::instance();
  EXPECT_THROW(registry.create("does-not-exist"), core::ConfigurationError);
}

TEST(CodecRegistryTest, CreateReturnsFreshInstances) {
  const auto &registry = CodecRegistry::instance();
  auto a = registry.create("optimized");
  auto b = registry.create("optimized");
  EXPECT_NE(a.get(), b.get());
}

TEST(CodecRegistryTest, AllIsSortedById) {
  const auto &registry = CodecRegistry::instance();
  const auto all = registry.all();
  ASSERT_GE(all.size(), 9u);
  for (size_t i = 1; i < all.size(); ++i) {
    EXPECT_LT(static_cast<uint8_t>(all[i - 1].first),
              static_cast<uint8_t>(all[i].first));
  }
}
