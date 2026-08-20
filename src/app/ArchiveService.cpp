#include <compression/app/ArchiveService.hpp>

#include <compression/Crc32.hpp>
#include <compression/archive/ArchiveReader.hpp>
#include <compression/archive/ArchiveWriter.hpp>
#include <compression/core/Errors.hpp>

#include <chrono>
#include <utility>

namespace compression {

ArchiveService::ArchiveService(std::shared_ptr<app::EventBus> events)
    : events_(std::move(events)) {}

void ArchiveService::publish(app::EventType type, uint64_t in, uint64_t out,
                             uint8_t progressPct) const {
  if (events_) {
    events_->publish(app::CompressionEvent{type, format::AlgorithmID::UNKNOWN,
                                           in, out, progressPct});
  }
}

void ArchiveService::create(const std::filesystem::path &outPath,
                            const archive::ArchiveBuildOptions &options,
                            const std::vector<ArchiveEntrySource> &entries) {
  using namespace std::chrono;
  const auto start = steady_clock::now();

  core::FileByteSink sink(outPath);
  archive::ArchiveWriter writer(sink, options);
  publish(app::EventType::OperationStarted, 0, 0, 0);
  for (const ArchiveEntrySource &entry : entries) {
    writer.addEntry(entry.name, core::ByteView(entry.data), entry.mtime);
  }
  writer.finalize();
  publish(app::EventType::OperationCompleted, 0, sink.size(), 100);
  (void)start;
}

archive::ArchiveListing
ArchiveService::list(const std::filesystem::path &archivePath) {
  core::FileByteSource source(archivePath);
  archive::ArchiveReader reader(source);
  return reader.listing();
}

ExtractResult ArchiveService::extract(const std::filesystem::path &archivePath,
                                      archive::EntryId id,
                                      const std::filesystem::path &outDir) {
  using namespace std::chrono;
  const auto start = steady_clock::now();

  core::FileByteSource source(archivePath);
  archive::ArchiveReader reader(source);
  const archive::ArchiveListing &listing = reader.listing();
  if (id >= listing.entries.size()) {
    throw std::out_of_range("Archive entry id out of range");
  }
  const archive::ArchiveEntry &entry = listing.entries[id];

  // Reject path traversal: no absolute paths, no ".." segments.
  const std::filesystem::path relative(entry.name);
  if (relative.is_absolute()) {
    throw core::IoError("Archive entry has an absolute path: " + entry.name);
  }
  for (const auto &part : relative) {
    if (part == "..") {
      throw core::IoError("Archive entry path escapes the output directory: " +
                          entry.name);
    }
  }

  core::MemoryByteSink buffer;
  reader.extract(id, buffer);
  const std::vector<uint8_t> &bytes = buffer.data();

  const std::filesystem::path target = outDir / relative;
  std::error_code ec;
  if (target.has_parent_path()) {
    std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) {
      throw core::IoError("Cannot create output directory: " +
                          target.parent_path().string());
    }
  }
  {
    core::FileByteSink sink(target);
    sink.write(core::ByteView(bytes));
  }

  const uint32_t crc = utils::crc32Calculator.calculate(bytes.data(), bytes.size());
  ExtractResult result;
  result.inBytes = entry.rawSize;
  result.outBytes = bytes.size();
  result.crc = crc;
  result.verified = (crc == entry.checksum);
  result.elapsed = duration_cast<milliseconds>(steady_clock::now() - start);
  return result;
}

std::vector<archive::BlockVerifyResult>
ArchiveService::verify(const std::filesystem::path &archivePath) {
  core::FileByteSource source(archivePath);
  archive::ArchiveReader reader(source);
  return reader.verify();
}

} // namespace compression
