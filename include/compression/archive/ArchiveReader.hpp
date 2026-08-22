#pragma once

#include <compression/archive/Archive.hpp>
#include <compression/core/ByteSource.hpp>

#include <cstdint>
#include <vector>

namespace compression {
namespace archive {

/**
 * @brief Reads a .cza archive via random access.
 *
 * Opens by seeking to the tail footer (no full decompression), loads the
 * file table and block table into memory, and can extract individual entries
 * by decompressing only the blocks they span.
 */
class ArchiveReader {
public:
  explicit ArchiveReader(core::IByteSource &source);

  const ArchiveListing &listing() const { return listing_; }

  /**
   * @brief Decompresses the blocks spanned by @p id and writes the entry's
   * bytes to @p out.
   * @throws CorruptDataError on block checksum/size mismatch or out-of-range
   * metadata; std::out_of_range for an unknown id.
   */
  void extract(EntryId id, core::IByteSink &out) const;

  /**
   * @brief Verifies every block: decompress + raw-size + CRC check.
   */
  std::vector<BlockVerifyResult> verify() const;

private:
  struct BlockRecord {
    format::AlgorithmID codec = format::AlgorithmID::UNKNOWN;
    uint64_t compOffset = 0;
    uint32_t compLen = 0;
    uint32_t rawLen = 0;
    uint32_t checksum = 0;
  };

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

  std::vector<uint8_t> readBlockData(uint32_t index) const;
  std::vector<uint8_t> decompressChunk(const std::vector<uint8_t> &compressed,
                                       uint32_t index) const;

  core::IByteSource &src_;
  ArchiveListing listing_;
  std::vector<EntryRecord> entries_;
  std::vector<BlockRecord> blocks_;
};

} // namespace archive
} // namespace compression
