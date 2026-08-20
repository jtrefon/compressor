#include <compression/archive/ArchiveReader.hpp>

#include <compression/Crc32.hpp>
#include <compression/SystemInfo.hpp>
#include <compression/ThreadPool.hpp>
#include <compression/codec/CodecRegistry.hpp>
#include <compression/core/Errors.hpp>

#include <algorithm>
#include <cstdint>
#include <future>
#include <stdexcept>
#include <string>
#include <utility>

namespace compression {
namespace archive {

namespace {

// Hostile-input hardening: caps on metadata size.
constexpr uint32_t kMaxEntries = 1 << 20;
constexpr uint32_t kMaxBlocks = 1 << 20;
constexpr uint64_t kFooterSize = 8 + 8 + 4 + 4 + 4; // 28 bytes

std::string readName(core::BinaryReader &reader) {
  const std::size_t length = reader.u16();
  const core::ByteView bytes = reader.readBytes(length);
  return std::string(bytes.begin(), bytes.end());
}

} // namespace

ArchiveReader::ArchiveReader(core::IByteSource &source) : src_(source) {
  const uint64_t fileSize = src_.size();
  if (fileSize < kFooterSize) {
    throw core::CorruptDataError("Archive too small to be a .cza file");
  }

  // 1. Tail footer -> table offsets.
  uint64_t fileTableOffset = 0;
  uint64_t blockTableOffset = 0;
  uint32_t entryCount = 0;
  uint32_t blockCount = 0;
  {
    src_.seek(fileSize - kFooterSize);
    core::BinaryReader reader(src_);
    fileTableOffset = reader.u64();
    blockTableOffset = reader.u64();
    entryCount = reader.u32();
    blockCount = reader.u32();
    reader.expectMagic(ARCHIVE_MAGIC);
  }
  if (fileTableOffset > fileSize || blockTableOffset > fileSize) {
    throw core::CorruptDataError("Archive table offset out of range");
  }
  if (entryCount > kMaxEntries) {
    throw core::CorruptDataError("Invalid entry count in archive");
  }
  if (blockCount > kMaxBlocks) {
    throw core::CorruptDataError("Invalid block count in archive");
  }

  // 2. Header (magic, version, block size, default codec).
  {
    src_.seek(0);
    core::BinaryReader reader(src_);
    reader.expectMagic(ARCHIVE_MAGIC);
    const uint8_t version = reader.u8();
    if (version != ARCHIVE_VERSION) {
      throw core::UnsupportedVersionError(
          "Unsupported archive version: " + std::to_string(version));
    }
    (void)reader.u8(); // flags, reserved for now
    listing_.blockSize = reader.u32();
    listing_.defaultCodec = static_cast<format::AlgorithmID>(reader.u8());
  }

  // 3. File table.
  {
    src_.seek(fileTableOffset);
    core::BinaryReader reader(src_);
    const uint32_t n = reader.u32();
    if (n != entryCount) {
      throw core::CorruptDataError("File table entry count mismatch");
    }
    entries_.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
      EntryRecord entry;
      entry.name = readName(reader);
      entry.rawSize = reader.u64();
      entry.mtime = reader.u64();
      entry.attrs = reader.u32();
      entry.checksum = reader.u32();
      entry.firstBlock = reader.u32();
      entry.blockCount = reader.u32();
      entry.offsetInFirstBlock = reader.u32();

      if (entry.firstBlock + entry.blockCount > blockCount) {
        throw core::CorruptDataError("Entry block range out of bounds");
      }
      entries_.push_back(std::move(entry));
    }
  }

  // 4. Block table.
  {
    src_.seek(blockTableOffset);
    core::BinaryReader reader(src_);
    const uint32_t m = reader.u32();
    if (m != blockCount) {
      throw core::CorruptDataError("Block table entry count mismatch");
    }
    blocks_.reserve(m);
    for (uint32_t i = 0; i < m; ++i) {
      BlockRecord block;
      block.codec = static_cast<format::AlgorithmID>(reader.u8());
      block.compOffset = reader.u64();
      block.compLen = reader.u32();
      block.rawLen = reader.u32();
      block.checksum = reader.u32();
      if (block.compOffset + block.compLen > fileSize || block.compLen == 0) {
        throw core::CorruptDataError("Block data out of range");
      }
      blocks_.push_back(std::move(block));
    }
  }

  // 5. Build the listing + totals.
  listing_.blockCount = blockCount;
  uint64_t totalCompressed = 0;
  for (const BlockRecord &block : blocks_) {
    totalCompressed += block.compLen;
  }
  listing_.totalCompressedSize = totalCompressed;

  listing_.entries.reserve(entries_.size());
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    const EntryRecord &e = entries_[i];
    ArchiveEntry entry;
    entry.id = static_cast<EntryId>(i);
    entry.name = e.name;
    entry.rawSize = e.rawSize;
    entry.mtime = e.mtime;
    entry.attrs = e.attrs;
    entry.checksum = e.checksum;
    listing_.entries.push_back(std::move(entry));
    listing_.totalRawSize += e.rawSize;
  }
}

std::vector<uint8_t> ArchiveReader::readBlockData(uint32_t index) const {
  const BlockRecord &block = blocks_[index];
  std::vector<uint8_t> data(block.compLen);
  src_.seek(block.compOffset);
  std::size_t got = 0;
  while (got < data.size()) {
    const std::size_t n = src_.read(data.data() + got, data.size() - got);
    if (n == 0) {
      throw core::CorruptDataError("Unexpected end of archive block data");
    }
    got += n;
  }
  return data;
}

std::vector<uint8_t> ArchiveReader::decompressChunk(
    const std::vector<uint8_t> &compressed, uint32_t index) const {
  const BlockRecord &block = blocks_[index];
  auto codec = codec::CodecRegistry::instance().create(block.codec);
  std::vector<uint8_t> raw = codec->decompress(compressed);
  if (raw.size() != block.rawLen) {
    throw core::CorruptDataError("Block decompressed size mismatch");
  }
  if (utils::crc32Calculator.calculate(raw.data(), raw.size()) !=
      block.checksum) {
    throw core::CorruptDataError("Block checksum mismatch");
  }
  return raw;
}

void ArchiveReader::extract(EntryId id, core::IByteSink &out) const {
  if (id >= entries_.size()) {
    throw std::out_of_range("Archive entry id out of range");
  }
  const EntryRecord &entry = entries_[id];
  if (entry.rawSize == 0) {
    return; // nothing to write
  }
  if (entry.blockCount == 0) {
    throw core::CorruptDataError("Entry has no blocks");
  }

  const std::size_t first = entry.firstBlock;
  const std::size_t count = entry.blockCount;

  // Read all compressed block data sequentially (sources are not
  // thread-safe), then decompress in parallel.
  std::vector<std::vector<uint8_t>> compressed(count);
  for (std::size_t k = 0; k < count; ++k) {
    compressed[k] = readBlockData(static_cast<uint32_t>(first + k));
  }

  const std::size_t threads = std::max<std::size_t>(
      1, std::min<std::size_t>(count, getHardwareThreads()));
  ThreadPool pool(threads);
  std::vector<std::future<std::vector<uint8_t>>> futures(count);
  for (std::size_t k = 0; k < count; ++k) {
    futures[k] = pool.submit([this, &compressed, first, k]() {
      return decompressChunk(compressed[k],
                             static_cast<uint32_t>(first + k));
    });
  }

  std::vector<uint8_t> raw;
  for (std::size_t k = 0; k < count; ++k) {
    std::vector<uint8_t> chunk = futures[k].get();
    raw.insert(raw.end(), chunk.begin(), chunk.end());
  }

  if (static_cast<uint64_t>(entry.offsetInFirstBlock) + entry.rawSize >
      raw.size()) {
    throw core::CorruptDataError("Entry range out of block data");
  }
  out.write(core::ByteView(raw.data() + entry.offsetInFirstBlock,
                           static_cast<std::size_t>(entry.rawSize)));
}

std::vector<BlockVerifyResult> ArchiveReader::verify() const {
  const std::size_t count = blocks_.size();
  if (count == 0) {
    return {};
  }

  // Read all block data sequentially (sources are not thread-safe), then
  // decompress and checksum in parallel.
  std::vector<std::vector<uint8_t>> compressed(count);
  for (std::size_t i = 0; i < count; ++i) {
    compressed[i] = readBlockData(static_cast<uint32_t>(i));
  }

  const std::size_t threads = std::max<std::size_t>(
      1, std::min<std::size_t>(count, getHardwareThreads()));
  ThreadPool pool(threads);
  std::vector<std::future<BlockVerifyResult>> futures(count);
  for (std::size_t i = 0; i < count; ++i) {
    futures[i] = pool.submit([this, i, &compressed]() {
      BlockVerifyResult result;
      result.blockIndex = static_cast<uint32_t>(i);
      result.rawLen = blocks_[i].rawLen;
      result.compLen = blocks_[i].compLen;
      try {
        (void)decompressChunk(compressed[i], static_cast<uint32_t>(i));
        result.ok = true;
      } catch (const std::exception &) {
        result.ok = false;
      }
      return result;
    });
  }
  std::vector<BlockVerifyResult> results(count);
  for (std::size_t i = 0; i < count; ++i) {
    results[i] = futures[i].get();
  }
  return results;
}

} // namespace archive
} // namespace compression
