#include <compression/app/ArchiveService.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

using namespace compression;

namespace {

class TempPath {
public:
  explicit TempPath(const std::string &name)
      : path_(std::filesystem::temp_directory_path() /
              ("compressor_m4svc_" + name + "_" +
               std::to_string(std::chrono::steady_clock::now()
                                  .time_since_epoch()
                                  .count()))) {}

  ~TempPath() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

std::vector<uint8_t> textData(size_t n) {
  std::vector<uint8_t> data;
  data.reserve(n);
  const std::string words[] = {"alpha", "bravo", "charlie", "delta"};
  size_t w = 0;
  while (data.size() < n) {
    for (char c : words[w % 4]) {
      data.push_back(static_cast<uint8_t>(c));
    }
    data.push_back(' ');
    w++;
  }
  data.resize(n);
  return data;
}

std::vector<ArchiveEntrySource> makeEntries() {
  return {
      {"docs/readme.txt", textData(4000)},
      {"docs/notes.txt", textData(800)},
      {"src/main.cpp", textData(2000)},
      {"empty.dat", {}},
  };
}

std::vector<uint8_t> readFile(const std::filesystem::path &path) {
  core::FileByteSource source(path);
  std::vector<uint8_t> out;
  std::vector<uint8_t> buf(4096);
  while (true) {
    const std::size_t n = source.read(buf.data(), buf.size());
    if (n == 0) {
      break;
    }
    out.insert(out.end(), buf.begin(), buf.begin() + n);
  }
  return out;
}

} // namespace

TEST(ArchiveServiceTest, CreateListExtractVerify) {
  ArchiveService service;
  TempPath archive("arc");
  TempPath outDir("out");

  archive::ArchiveBuildOptions options;
  options.blockSize = 1024;

  service.create(archive.path(), options, makeEntries());

  const archive::ArchiveListing listing = service.list(archive.path());
  ASSERT_EQ(listing.entries.size(), 4u);
  EXPECT_EQ(listing.entries[0].name, "docs/readme.txt");
  EXPECT_EQ(listing.entries[0].rawSize, 4000u);

  // Random access: extract only the third entry.
  ExtractResult result = service.extract(archive.path(), 2, outDir.path());
  EXPECT_TRUE(result.verified);
  EXPECT_EQ(result.outBytes, 2000u);
  const auto extracted = readFile(outDir.path() / "src/main.cpp");
  EXPECT_EQ(extracted, textData(2000));

  const auto verifyResults = service.verify(archive.path());
  ASSERT_FALSE(verifyResults.empty());
  for (const auto &r : verifyResults) {
    EXPECT_TRUE(r.ok) << "block " << r.blockIndex;
  }
}

TEST(ArchiveServiceTest, ExtractPreservesSubdirectories) {
  ArchiveService service;
  TempPath archive("arc");
  TempPath outDir("out");

  service.create(archive.path(), {}, makeEntries());
  service.extract(archive.path(), 0, outDir.path());

  const auto path = outDir.path() / "docs/readme.txt";
  EXPECT_TRUE(std::filesystem::exists(path));
  EXPECT_EQ(readFile(path), textData(4000));
}

TEST(ArchiveServiceTest, CorruptBlockVerifyReportsFailure) {
  ArchiveService service;
  TempPath archive("arc");
  TempPath outDir("out");

  service.create(archive.path(), {}, makeEntries());

  // Corrupt a byte inside the first block's compressed payload (12-byte
  // container header precedes the block data).
  {
    std::vector<uint8_t> bytes = readFile(archive.path());
    ASSERT_GT(bytes.size(), 20u);
    bytes[16] ^= 0xFF;
    core::FileByteSink sink(archive.path());
    sink.write(core::ByteView(bytes));
  }

  const auto results = service.verify(archive.path());
  bool anyFailed = false;
  for (const auto &r : results) {
    anyFailed = anyFailed || !r.ok;
  }
  EXPECT_TRUE(anyFailed);
}

TEST(ArchiveServiceTest, PathTraversalRejected) {
  ArchiveService service;
  TempPath archive("arc");
  TempPath outDir("out");

  service.create(archive.path(), {}, {{"../evil.txt", textData(100)}});
  EXPECT_THROW(service.extract(archive.path(), 0, outDir.path()),
               core::IoError);
}

TEST(ArchiveServiceTest, AbsolutePathRejected) {
  ArchiveService service;
  TempPath archive("arc");
  TempPath outDir("out");

  service.create(archive.path(), {},
                 {{"/etc/passwd", textData(100)}});
  EXPECT_THROW(service.extract(archive.path(), 0, outDir.path()),
               core::IoError);
}

TEST(ArchiveServiceTest, UnknownEntryIdThrows) {
  ArchiveService service;
  TempPath archive("arc");
  TempPath outDir("out");

  service.create(archive.path(), {}, makeEntries());
  EXPECT_THROW(service.extract(archive.path(), 99, outDir.path()),
               std::out_of_range);
}

TEST(ArchiveServiceTest, NotAnArchiveThrowsOnList) {
  ArchiveService service;
  TempPath archive("arc");
  {
    core::FileByteSink sink(archive.path());
    sink.write(core::ByteView(std::vector<uint8_t>{'n', 'o', 't', ' '}));
  }
  EXPECT_THROW(service.list(archive.path()), core::CompressionError);
}
