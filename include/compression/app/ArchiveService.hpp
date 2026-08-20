#pragma once

#include <compression/app/CompressionService.hpp>
#include <compression/app/EventBus.hpp>
#include <compression/archive/Archive.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace compression {

/**
 * @brief An entry to add to an archive (value object).
 */
struct ArchiveEntrySource {
  std::string name;
  std::vector<uint8_t> data;
  uint64_t mtime = 0;
};

/**
 * @brief Facade for .cza archive operations.
 *
 * Sits on top of ArchiveWriter/ArchiveReader and file IO; emits events on the
 * injected bus. Entry names are validated against path traversal on extract.
 */
class ArchiveService {
public:
  explicit ArchiveService(std::shared_ptr<app::EventBus> events = nullptr);

  /**
   * @brief Creates an archive at @p outPath from @p entries.
   */
  void create(const std::filesystem::path &outPath,
              const archive::ArchiveBuildOptions &options,
              const std::vector<ArchiveEntrySource> &entries);

  /**
   * @brief Opens an archive and returns its listing (no decompression).
   * @throws InvalidFormatError/CorruptDataError on bad structure.
   */
  archive::ArchiveListing list(const std::filesystem::path &archivePath);

  /**
   * @brief Extracts one entry to @p outDir preserving its archive path.
   * Rejects absolute paths and ".." traversal.
   * @return ExtractResult with in/out bytes, crc and elapsed.
   */
  ExtractResult extract(const std::filesystem::path &archivePath,
                        archive::EntryId id,
                        const std::filesystem::path &outDir);

  /**
   * @brief Verifies every block of an archive.
   */
  std::vector<archive::BlockVerifyResult>
  verify(const std::filesystem::path &archivePath);

  std::shared_ptr<app::EventBus> events() const { return events_; }

private:
  void publish(app::EventType type, uint64_t in, uint64_t out,
               uint8_t progressPct) const;

  std::shared_ptr<app::EventBus> events_;
};

} // namespace compression
