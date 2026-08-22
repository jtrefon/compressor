#pragma once

#include <compression/FileFormat.hpp>
#include <compression/core/BinaryIO.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace compression {
namespace archive {

/**
 * @brief Magic of the seekable block-indexed archive container ("CZA1").
 */
inline constexpr core::FourCC ARCHIVE_MAGIC = core::makeFourCC('C', 'Z', 'A', '1');

/**
 * @brief Current .cza format version.
 */
inline constexpr uint8_t ARCHIVE_VERSION = 1;

/**
 * @brief Stable identifier of an entry within an archive.
 */
using EntryId = uint32_t;

/**
 * @brief One entry in an archive listing (value object).
 */
struct ArchiveEntry {
  EntryId id = 0;
  std::string name; // path within the archive, UTF-8, '/' separators
  uint64_t rawSize = 0;
  uint64_t mtime = 0; // seconds since epoch, 0 if unknown
  uint32_t attrs = 0;
  uint32_t checksum = 0; // CRC32 of the entry's raw bytes
};

/**
 * @brief Snapshot of an opened archive (value object).
 */
struct ArchiveListing {
  std::vector<ArchiveEntry> entries;
  uint32_t blockCount = 0;
  uint64_t totalRawSize = 0;
  uint64_t totalCompressedSize = 0;
  format::AlgorithmID defaultCodec = format::AlgorithmID::OPTIMIZED_COMPRESSOR;
  uint32_t blockSize = 0;
};

/**
 * @brief Options for building an archive.
 */
struct ArchiveBuildOptions {
  format::AlgorithmID codec = format::AlgorithmID::OPTIMIZED_COMPRESSOR;
  uint32_t blockSize = 1 << 20; // 1 MiB
  std::size_t threads = 1;
};

/**
 * @brief Outcome of verifying one block (value object).
 */
struct BlockVerifyResult {
  uint32_t blockIndex = 0;
  bool ok = false;
  uint32_t rawLen = 0;
  uint32_t compLen = 0;
};

} // namespace archive
} // namespace compression
