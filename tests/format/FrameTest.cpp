#include <compression/FileFormat.hpp>
#include <compression/format/FrameRegistry.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using namespace compression;
using namespace compression::format;
using namespace compression::core;

namespace {

FileHeader sampleHeader() {
  FileHeader header;
  header.formatVersion = FORMAT_VERSION;
  header.algorithmId = AlgorithmID::OPTIMIZED_COMPRESSOR;
  header.originalSize = 100;
  header.originalChecksum = 0xDEADBEEF;
  header.chunkCount = 2;
  header.chunkSize = 50;
  header.compressedSizes = {40, 42};
  return header;
}

} // namespace

TEST(FrameTest, LegacyMagicIsCpro) {
  EXPECT_EQ(LEGACY_FRAME_MAGIC, makeFourCC('C', 'P', 'R', 'O'));
  EXPECT_EQ(MAGIC_NUMBER,
            (std::array<uint8_t, 4>{'C', 'P', 'R', 'O'}));
}

TEST(FrameTest, EncodeHeaderIsByteIdenticalToLegacyLayout) {
  // Golden fixture: exact bytes the 1.x serializer produced
  // (magic, version, algo, size LE, crc LE, chunkCount LE, chunkSize LE,
  //  per-chunk sizes LE).
  const std::vector<uint8_t> expected = {
      'C', 'P', 'R', 'O',    // magic
      0x01,                  // format version
      0x07,                  // OPTIMIZED_COMPRESSOR
      0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 100 LE
      0xEF, 0xBE, 0xAD, 0xDE,                         // 0xDEADBEEF LE
      0x02, 0x00, 0x00, 0x00,                         // 2 chunks
      0x32, 0x00, 0x00, 0x00,                         // chunk size 50
      0x28, 0x00, 0x00, 0x00,                         // size[0] = 40
      0x2A, 0x00, 0x00, 0x00,                         // size[1] = 42
  };
  EXPECT_EQ(serializeHeader(sampleHeader()), expected);
}

TEST(FrameTest, DecodeHeaderRoundTrip) {
  const FileHeader header = sampleHeader();
  const FileHeader decoded = deserializeHeader(serializeHeader(header));
  EXPECT_EQ(decoded.formatVersion, header.formatVersion);
  EXPECT_EQ(decoded.algorithmId, header.algorithmId);
  EXPECT_EQ(decoded.originalSize, header.originalSize);
  EXPECT_EQ(decoded.originalChecksum, header.originalChecksum);
  EXPECT_EQ(decoded.chunkCount, header.chunkCount);
  EXPECT_EQ(decoded.chunkSize, header.chunkSize);
  EXPECT_EQ(decoded.compressedSizes, header.compressedSizes);
}

TEST(FrameTest, BadMagicThrowsInvalidFormatError) {
  std::vector<uint8_t> bytes = serializeHeader(sampleHeader());
  bytes[0] = 'X';
  EXPECT_THROW(deserializeHeader(bytes), InvalidFormatError);
}

TEST(FrameTest, BadVersionThrowsUnsupportedVersionError) {
  std::vector<uint8_t> bytes = serializeHeader(sampleHeader());
  bytes[4] = 0x63;
  EXPECT_THROW(deserializeHeader(bytes), UnsupportedVersionError);
}

TEST(FrameTest, TruncatedHeaderThrowsCorruptDataError) {
  std::vector<uint8_t> bytes = serializeHeader(sampleHeader());
  bytes.resize(bytes.size() - 1);
  EXPECT_THROW(deserializeHeader(bytes), CorruptDataError);
}

TEST(FrameTest, EmptyBufferThrowsCorruptDataError) {
  std::vector<uint8_t> bytes;
  EXPECT_THROW(deserializeHeader(bytes), CorruptDataError);
}

TEST(FrameTest, ZeroChunkCountRejected) {
  FileHeader header = sampleHeader();
  header.chunkCount = 0;
  header.compressedSizes.clear();
  EXPECT_THROW(deserializeHeader(serializeHeader(header)), CorruptDataError);
}

TEST(FrameTest, AbsurdChunkCountRejected) {
  FileHeader header = sampleHeader();
  header.chunkCount = 1000000;
  header.compressedSizes.assign(1000000, 4);
  EXPECT_THROW(deserializeHeader(serializeHeader(header)), CorruptDataError);
}

TEST(FrameTest, RegistryFindAndRequire) {
  const auto &registry = FrameRegistry::instance();
  EXPECT_NE(registry.find(LEGACY_FRAME_MAGIC), nullptr);
  EXPECT_EQ(registry.find(makeFourCC('N', 'O', 'P', 'E')), nullptr);
  EXPECT_NE(registry.require(LEGACY_FRAME_MAGIC), nullptr);
  EXPECT_THROW(registry.require(makeFourCC('N', 'O', 'P', 'E')),
               InvalidFormatError);
}

TEST(FrameTest, ErrorsDeriveFromRuntimeError) {
  // Existing 1.x catch-sites that catch std::runtime_error keep working.
  const std::vector<uint8_t> junk = {0x00};
  try {
    deserializeHeader(junk);
    FAIL() << "expected exception";
  } catch (const std::runtime_error &) {
    // expected
  }
}
