#include <compression/archive/ArchiveWriter.hpp>

#include <compression/Crc32.hpp>
#include <compression/codec/CodecRegistry.hpp>
#include <compression/core/Errors.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace compression {
namespace archive {

ArchiveWriter::ArchiveWriter(core::IByteSink &sink,
                             const ArchiveBuildOptions &options)
    : sink_(sink), options_(options) {
  if (options_.blockSize == 0) {
    throw core::ConfigurationError("Archive block size must be non-zero");
  }
  // Container header (12 bytes) at offset 0.
  core::BinaryWriter writer(sink_);
  writer.magic(ARCHIVE_MAGIC);
  writer.u8(ARCHIVE_VERSION);
  writer.u8(0); // flags, reserved for now
  writer.u32(options_.blockSize);
  writer.u8(static_cast<uint8_t>(options_.codec));
  writer.u8(0); // reserved
}

void ArchiveWriter::writeBlock(core::ByteView raw) {
  auto codec = codec::CodecRegistry::instance().create(options_.codec);
  std::vector<uint8_t> compressed =
      codec->compress(std::vector<uint8_t>(raw.begin(), raw.end()));

  BlockRecord record;
  record.codec = options_.codec;
  record.compOffset = sink_.size();
  record.compLen = static_cast<uint32_t>(compressed.size());
  record.rawLen = static_cast<uint32_t>(raw.size());
  record.checksum = utils::crc32Calculator.calculate(raw.data(), raw.size());
  blocks_.push_back(record);

  sink_.write(core::ByteView(compressed));
}

void ArchiveWriter::flushBlock() {
  if (blockBuf_.empty()) {
    return;
  }
  writeBlock(core::ByteView(blockBuf_));
  blockBuf_.clear();
}

void ArchiveWriter::addEntry(const std::string &name, core::ByteView data,
                             uint64_t mtime, uint32_t attrs) {
  if (name.empty()) {
    throw core::ConfigurationError("Archive entry name must not be empty");
  }
  if (name.size() > 65535) {
    throw core::ConfigurationError("Archive entry name too long");
  }

  EntryRecord entry;
  entry.name = name;
  entry.rawSize = data.size();
  entry.mtime = mtime;
  entry.attrs = attrs;
  entry.checksum = utils::crc32Calculator.calculate(data.data(), data.size());
  entry.firstBlock = static_cast<uint32_t>(blocks_.size());
  entry.offsetInFirstBlock = static_cast<uint32_t>(blockBuf_.size());

  std::size_t consumed = 0;
  while (consumed < data.size()) {
    if (blockBuf_.size() == options_.blockSize) {
      flushBlock();
    }
    const std::size_t space = options_.blockSize - blockBuf_.size();
    const std::size_t take = std::min(space, data.size() - consumed);
    const core::ByteView chunk = data.subspan(consumed, take);

    if (blockBuf_.empty() && take == options_.blockSize) {
      // A standalone full block: avoid buffering a copy.
      writeBlock(chunk);
    } else {
      blockBuf_.insert(blockBuf_.end(), chunk.begin(), chunk.end());
      if (blockBuf_.size() == options_.blockSize) {
        flushBlock();
      }
    }
    consumed += take;
  }

  entry.blockCount =
      static_cast<uint32_t>(blocks_.size() - entry.firstBlock +
                            (blockBuf_.empty() ? 0 : 1));
  entries_.push_back(std::move(entry));
}

void ArchiveWriter::finalize() {
  if (finalized_) {
    throw std::logic_error("ArchiveWriter::finalize called twice");
  }
  flushBlock();

  const uint64_t fileTableOffset = sink_.size();
  {
    core::BinaryWriter writer(sink_);
    writer.u32(static_cast<uint32_t>(entries_.size()));
    for (const EntryRecord &entry : entries_) {
      writer.u16(static_cast<uint16_t>(entry.name.size()));
      writer.bytes(core::ByteView(entry.name));
      writer.u64(entry.rawSize);
      writer.u64(entry.mtime);
      writer.u32(entry.attrs);
      writer.u32(entry.checksum);
      writer.u32(entry.firstBlock);
      writer.u32(entry.blockCount);
      writer.u32(entry.offsetInFirstBlock);
    }
  }

  const uint64_t blockTableOffset = sink_.size();
  {
    core::BinaryWriter writer(sink_);
    writer.u32(static_cast<uint32_t>(blocks_.size()));
    for (const BlockRecord &block : blocks_) {
      writer.u8(static_cast<uint8_t>(block.codec));
      writer.u64(block.compOffset);
      writer.u32(block.compLen);
      writer.u32(block.rawLen);
      writer.u32(block.checksum);
    }
  }

  {
    core::BinaryWriter writer(sink_);
    writer.u64(fileTableOffset);
    writer.u64(blockTableOffset);
    writer.u32(static_cast<uint32_t>(entries_.size()));
    writer.u32(static_cast<uint32_t>(blocks_.size()));
    writer.magic(ARCHIVE_MAGIC);
  }

  finalized_ = true;
}

} // namespace archive
} // namespace compression
