#pragma once

#include <compression/archive/Archive.hpp>
#include <compression/core/ByteSource.hpp>

#include <cstdint>
#include <string>

namespace compression {
namespace archive {

/**
 * @brief Writes a seekable block-indexed .cza archive.
 *
 * Entries are packed into fixed-size blocks in insertion order; each block is
 * compressed independently with a codec from the CodecRegistry. Metadata
 * (file table + block table) is written after the block payloads, ZIP-style,
 * so the reader can open an archive by seeking to its tail.
 */
class ArchiveWriter {
public:
  explicit ArchiveWriter(core::IByteSink &sink,
                         const ArchiveBuildOptions &options = {});

  /**
   * @brief Appends an entry. Data is copied into the block buffer.
   * @throws ConfigurationError if the name is empty or longer than 65535.
   */
  void addEntry(const std::string &name, core::ByteView data,
                uint64_t mtime = 0, uint32_t attrs = 0);

  /**
   * @brief Flushes remaining blocks and writes the file/block tables + footer.
   * Idempotence: throws std::logic_error if called twice.
   */
  void finalize();

private:
  struct EntryRecord {
    std::string name;
    uint64_t rawSize = 0;
    uint64_t mtime = 0;
    uint32_t attrs = 0;
    uint32_t checksum = 0;
    uint32_t firstBlock = 0;
    uint32_t blockCount = 0;
    uint32_t offsetInFirstBlock = 0;
  };

  struct BlockRecord {
    format::AlgorithmID codec = format::AlgorithmID::UNKNOWN;
    uint64_t compOffset = 0;
    uint32_t compLen = 0;
    uint32_t rawLen = 0;
    uint32_t checksum = 0;
  };

  void writeBlock(core::ByteView raw);
  void flushBlock();

  core::IByteSink &sink_;
  ArchiveBuildOptions options_;
  std::vector<uint8_t> blockBuf_;
  std::vector<EntryRecord> entries_;
  std::vector<BlockRecord> blocks_;
  bool finalized_ = false;
};

} // namespace archive
} // namespace compression
