#include <compression/core/BinaryIO.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <vector>

using namespace compression::core;

namespace {

std::vector<uint8_t> writtenBy(const std::function<void(BinaryWriter &)> &fn) {
  MemoryByteSink sink;
  BinaryWriter writer(sink);
  fn(writer);
  return sink.data();
}

} // namespace

TEST(BinaryWriterTest, U32IsLittleEndian) {
  const auto bytes = writtenBy([](BinaryWriter &w) { w.u32(0x04030201u); });
  const std::vector<uint8_t> expected = {0x01, 0x02, 0x03, 0x04};
  EXPECT_EQ(bytes, expected);
}

TEST(BinaryWriterTest, U64IsLittleEndian) {
  const auto bytes = writtenBy(
      [](BinaryWriter &w) { w.u64(0x0102030405060708ull); });
  const std::vector<uint8_t> expected = {0x08, 0x07, 0x06, 0x05,
                                         0x04, 0x03, 0x02, 0x01};
  EXPECT_EQ(bytes, expected);
}

TEST(BinaryWriterTest, U16IsLittleEndian) {
  const auto bytes = writtenBy([](BinaryWriter &w) { w.u16(0x0201u); });
  const std::vector<uint8_t> expected = {0x01, 0x02};
  EXPECT_EQ(bytes, expected);
}

TEST(BinaryWriterTest, MagicWritesFourBytes) {
  const auto bytes = writtenBy(
      [](BinaryWriter &w) { w.magic(makeFourCC('C', 'P', 'R', 'O')); });
  const std::vector<uint8_t> expected = {'C', 'P', 'R', 'O'};
  EXPECT_EQ(bytes, expected);
}

TEST(BinaryWriterTest, TracksSize) {
  MemoryByteSink sink;
  BinaryWriter writer(sink);
  writer.u8(1);
  writer.u16(2);
  writer.u32(3);
  writer.bytes(ByteView(std::vector<uint8_t>{9, 9}));
  EXPECT_EQ(writer.size(), 9u);
  EXPECT_EQ(sink.size(), 9u);
}

TEST(FourCCTest, ValuesAreDistinct) {
  EXPECT_NE(makeFourCC('A', 'B', 'C', 'D'), makeFourCC('A', 'B', 'C', 'E'));
  EXPECT_EQ(makeFourCC('A', 'B', 'C', 'D'), makeFourCC('A', 'B', 'C', 'D'));
}

TEST(BinaryReaderTest, RoundTripAllWidths) {
  std::vector<uint8_t> payload = writtenBy([](BinaryWriter &w) {
    w.u8(0xAB);
    w.u16(0xABCD);
    w.u32(0xDEADBEEFu);
    w.u64(0x0123456789ABCDEFull);
  });

  MemoryByteSource source(payload);
  BinaryReader reader(source);
  EXPECT_EQ(reader.u8(), 0xAB);
  EXPECT_EQ(reader.u16(), 0xABCD);
  EXPECT_EQ(reader.u32(), 0xDEADBEEFu);
  EXPECT_EQ(reader.u64(), 0x0123456789ABCDEFull);
  EXPECT_EQ(reader.remaining(), 0u);
}

TEST(BinaryReaderTest, TruncatedStreamThrows) {
  const std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
  MemoryByteSource source(payload);
  BinaryReader reader(source);
  EXPECT_THROW(reader.u32(), CorruptDataError);
}

TEST(BinaryReaderTest, EmptyStreamThrows) {
  const std::vector<uint8_t> payload;
  MemoryByteSource source(payload);
  BinaryReader reader(source);
  EXPECT_THROW(reader.u8(), CorruptDataError);
}

TEST(BinaryReaderTest, ExpectMagicMatches) {
  std::vector<uint8_t> payload = writtenBy(
      [](BinaryWriter &w) { w.magic(makeFourCC('C', 'P', 'R', 'O')); });
  MemoryByteSource source(payload);
  BinaryReader reader(source);
  EXPECT_NO_THROW(reader.expectMagic(makeFourCC('C', 'P', 'R', 'O')));
  EXPECT_EQ(reader.position(), 4u);
}

TEST(BinaryReaderTest, ExpectMagicMismatchThrows) {
  std::vector<uint8_t> payload = writtenBy(
      [](BinaryWriter &w) { w.magic(makeFourCC('X', 'Y', 'Z', '!')); });
  MemoryByteSource source(payload);
  BinaryReader reader(source);
  EXPECT_THROW(reader.expectMagic(makeFourCC('C', 'P', 'R', 'O')),
               InvalidFormatError);
}

TEST(BinaryReaderTest, TryMagicConsumesOnMatchAndMismatch) {
  std::vector<uint8_t> payload = writtenBy(
      [](BinaryWriter &w) { w.magic(makeFourCC('C', 'P', 'R', 'O')); });
  MemoryByteSource source(payload);
  BinaryReader reader(source);
  EXPECT_TRUE(reader.tryMagic(makeFourCC('C', 'P', 'R', 'O')));
  EXPECT_EQ(reader.position(), 4u);
}

TEST(BinaryReaderTest, ReadBytesAndSkip) {
  std::vector<uint8_t> payload = writtenBy([](BinaryWriter &w) {
    w.u32(1);
    w.u32(2);
    w.u32(3);
  });
  MemoryByteSource source(payload);
  BinaryReader reader(source);
  EXPECT_EQ(reader.u32(), 1u);
  reader.skip(4);
  EXPECT_EQ(reader.u32(), 3u);
  EXPECT_EQ(reader.remaining(), 0u);
}
