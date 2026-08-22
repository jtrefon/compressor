#include <compression/archive/Archive.hpp>
#include <compression/archive/ArchiveReader.hpp>
#include <compression/archive/ArchiveWriter.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace compression;
using namespace compression::archive;

namespace {

std::vector<uint8_t> patternData(size_t n, uint8_t seed) {
  std::vector<uint8_t> data(n);
  for (size_t i = 0; i < n; ++i) {
    data[i] = static_cast<uint8_t>(seed + (i % 251));
  }
  return data;
}

// Builds an archive in memory and returns the serialized bytes.
std::vector<uint8_t> buildArchive(const ArchiveBuildOptions &options,
                                  const std::vector<std::pair<std::string,
                                                              std::vector<uint8_t>>> &inputs) {
  core::MemoryByteSink sink;
  ArchiveWriter writer(sink, options);
  for (const auto &[name, data] : inputs) {
    writer.addEntry(name, core::ByteView(data));
  }
  writer.finalize();
  return sink.data();
}

} // namespace

TEST(ArchiveTest, ListRoundTrip) {
  const auto bytes = buildArchive(ArchiveBuildOptions{},
                                  {{"a.txt", patternData(100, 1)},
                                   {"b.bin", patternData(200, 2)}});
  core::MemoryByteSource source(bytes);
  ArchiveReader reader(source);
  const ArchiveListing &listing = reader.listing();

  ASSERT_EQ(listing.entries.size(), 2u);
  EXPECT_EQ(listing.entries[0].name, "a.txt");
  EXPECT_EQ(listing.entries[0].rawSize, 100u);
  EXPECT_EQ(listing.entries[1].name, "b.bin");
  EXPECT_EQ(listing.entries[1].rawSize, 200u);
  EXPECT_GT(listing.totalRawSize, 0u);
  EXPECT_GT(listing.blockCount, 0u);
}

TEST(ArchiveTest, ExtractEveryEntry) {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> inputs = {
      {"a.txt", patternData(500, 1)},
      {"b.bin", patternData(1200, 7)},
      {"c.dat", patternData(64, 3)},
  };
  const auto bytes = buildArchive(ArchiveBuildOptions{}, inputs);

  core::MemoryByteSource source(bytes);
  ArchiveReader reader(source);
  for (size_t i = 0; i < inputs.size(); ++i) {
    core::MemoryByteSink out;
    reader.extract(static_cast<EntryId>(i), out);
    EXPECT_EQ(out.data(), inputs[i].second) << "entry " << i;
  }
}

TEST(ArchiveTest, MultiBlockEntrySpansBlocks) {
  // 3 KiB entry with tiny 256-byte blocks -> spans many blocks.
  ArchiveBuildOptions options;
  options.blockSize = 256;
  const auto data = patternData(3000, 42);
  const auto bytes = buildArchive(options, {{"big", data}});

  core::MemoryByteSource source(bytes);
  ArchiveReader reader(source);
  EXPECT_GT(reader.listing().blockCount, 1u);
  core::MemoryByteSink out;
  reader.extract(0, out);
  EXPECT_EQ(out.data(), data);
}

TEST(ArchiveTest, MultipleEntriesShareBlocks) {
  // Several small entries packed into few blocks.
  ArchiveBuildOptions options;
  options.blockSize = 1024;
  std::vector<std::pair<std::string, std::vector<uint8_t>>> inputs;
  for (int i = 0; i < 50; ++i) {
    inputs.emplace_back("f" + std::to_string(i), patternData(10, static_cast<uint8_t>(i)));
  }
  const auto bytes = buildArchive(options, inputs);

  core::MemoryByteSource source(bytes);
  ArchiveReader reader(source);
  EXPECT_LT(reader.listing().blockCount, 50u);
  for (size_t i = 0; i < inputs.size(); ++i) {
    core::MemoryByteSink out;
    reader.extract(static_cast<EntryId>(i), out);
    EXPECT_EQ(out.data(), inputs[i].second) << "entry " << i;
  }
}

TEST(ArchiveTest, EmptyEntry) {
  const auto bytes = buildArchive(ArchiveBuildOptions{},
                                  {{"empty", {}}, {"real", patternData(50, 9)}});
  core::MemoryByteSource source(bytes);
  ArchiveReader reader(source);
  EXPECT_EQ(reader.listing().entries.size(), 2u);

  core::MemoryByteSink out;
  reader.extract(0, out);
  EXPECT_TRUE(out.data().empty());
}

TEST(ArchiveTest, EmptyArchive) {
  const auto bytes = buildArchive(ArchiveBuildOptions{}, {});
  core::MemoryByteSource source(bytes);
  ArchiveReader reader(source);
  EXPECT_TRUE(reader.listing().entries.empty());
}

TEST(ArchiveTest, MetadataPreserved) {
  core::MemoryByteSink sink;
  ArchiveWriter writer(sink, {});
  writer.addEntry("x", core::ByteView(patternData(10, 1)), 123456789, 0x1F);
  writer.finalize();

  core::MemoryByteSource source(sink.data());
  ArchiveReader reader(source);
  ASSERT_EQ(reader.listing().entries.size(), 1u);
  EXPECT_EQ(reader.listing().entries[0].mtime, 123456789u);
  EXPECT_EQ(reader.listing().entries[0].attrs, 0x1Fu);
  EXPECT_NE(reader.listing().entries[0].checksum, 0u);
}

TEST(ArchiveTest, VerifyAllBlocksOk) {
  const auto bytes = buildArchive(ArchiveBuildOptions{},
                                  {{"a", patternData(3000, 1)},
                                   {"b", patternData(1000, 2)}});
  core::MemoryByteSource source(bytes);
  ArchiveReader reader(source);
  const auto results = reader.verify();
  ASSERT_EQ(results.size(), reader.listing().blockCount);
  for (const auto &r : results) {
    EXPECT_TRUE(r.ok) << "block " << r.blockIndex;
  }
}

TEST(ArchiveTest, CorruptBlockDetectedByVerify) {
  const auto bytes = buildArchive(ArchiveBuildOptions{},
                                  {{"a", patternData(3000, 1)}});
  // Corrupt a byte in the middle of the block payload (after the 12-byte header).
  std::vector<uint8_t> corrupted = bytes;
  corrupted[corrupted.size() / 2] ^= 0xFF;

  core::MemoryByteSource source(corrupted);
  ArchiveReader reader(source);
  const auto results = reader.verify();
  ASSERT_FALSE(results.empty());
  EXPECT_FALSE(results[0].ok);
}

TEST(ArchiveTest, CorruptBlockThrowsOnExtract) {
  const auto bytes = buildArchive(ArchiveBuildOptions{},
                                  {{"a", patternData(2000, 1)}});
  std::vector<uint8_t> corrupted = bytes;
  corrupted[corrupted.size() / 2] ^= 0xFF;

  core::MemoryByteSource source(corrupted);
  ArchiveReader reader(source);
  core::MemoryByteSink out;
  EXPECT_THROW(reader.extract(0, out), core::CorruptDataError);
}

TEST(ArchiveTest, CorruptFooterMagicRejected) {
  const auto bytes = buildArchive(ArchiveBuildOptions{},
                                  {{"a", patternData(100, 1)}});
  std::vector<uint8_t> corrupted = bytes;
  corrupted.back() ^= 0xFF; // corrupt mirror magic

  core::MemoryByteSource source(corrupted);
  EXPECT_THROW(ArchiveReader reader(source), core::InvalidFormatError);
}

TEST(ArchiveTest, NotAnArchiveRejected) {
  const std::vector<uint8_t> garbage = {'g', 'a', 'r', 'b', 'a', 'g', 'e'};
  core::MemoryByteSource source(garbage);
  EXPECT_THROW(ArchiveReader reader(source), core::CorruptDataError);
}

TEST(ArchiveTest, UnknownEntryIdThrows) {
  const auto bytes = buildArchive(ArchiveBuildOptions{},
                                  {{"a", patternData(100, 1)}});
  core::MemoryByteSource source(bytes);
  ArchiveReader reader(source);
  core::MemoryByteSink out;
  EXPECT_THROW(reader.extract(99, out), std::out_of_range);
}

TEST(ArchiveTest, FinalizeTwiceThrows) {
  core::MemoryByteSink sink;
  ArchiveWriter writer(sink, {});
  writer.finalize();
  EXPECT_THROW(writer.finalize(), std::logic_error);
}

TEST(ArchiveTest, EmptyEntryNameRejected) {
  core::MemoryByteSink sink;
  ArchiveWriter writer(sink, {});
  EXPECT_THROW(writer.addEntry("", core::ByteView(patternData(10, 1))),
               core::ConfigurationError);
}
