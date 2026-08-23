// Consumer smoke test: builds against the INSTALLED CompressionLib package
// (find_package + CompressionLib::compression), proving library encapsulation.
#include <compression/app/CompressionService.hpp>
#include <compression/app/ArchiveService.hpp>
#include <compression/codec/CodecRegistry.hpp>
#include <compression/core/ByteSource.hpp>

#include <cstdio>
#include <string>
#include <vector>

int main() {
  using namespace compression;

  std::printf("registry codecs: ");
  for (const auto &[id, name] : codec::CodecRegistry::instance().all()) {
    std::printf("%s ", name.c_str());
  }
  std::printf("\n");

  const std::string text = "encapsulated archive engine, round trip #1, "
                           "round trip #2, round trip #3\n";
  const std::vector<uint8_t> data(text.begin(), text.end());

  CompressionService service;
  core::MemoryByteSink sink;
  const CompressResult cr = service.compress(core::ByteView(data), sink);
  if (cr.outBytes == 0 || cr.crc == 0) {
    std::printf("FAIL: compress result empty\n");
    return 1;
  }

  core::MemoryByteSink out;
  const ExtractResult er =
      service.decompress(core::ByteView(sink.data()), out);
  if (!er.verified || out.data() != data) {
    std::printf("FAIL: round trip mismatch\n");
    return 1;
  }
  std::printf("compress/decompress round trip OK (%llu -> %llu bytes)\n",
              (unsigned long long)cr.inBytes, (unsigned long long)cr.outBytes);

  archive::ArchiveBuildOptions opts;
  std::vector<ArchiveEntrySource> entries;
  entries.push_back({"a.txt", data, 0});
  entries.push_back({"sub/b.bin", std::vector<uint8_t>(1000, 0xAB), 0});

  const std::string archPath = "/tmp/consumer_test.cza";
  ArchiveService archiveService;
  archiveService.create(archPath, opts, entries);

  const auto listing = archiveService.list(archPath);
  if (listing.entries.size() != 2) {
    std::printf("FAIL: listing size %zu\n", listing.entries.size());
    return 1;
  }
  const auto verifyResults = archiveService.verify(archPath);
  for (const auto &v : verifyResults) {
    if (!v.ok) {
      std::printf("FAIL: block verify\n");
      return 1;
    }
  }
  std::printf("archive create/list/verify OK (%zu entries, %zu blocks)\n",
              listing.entries.size(), verifyResults.size());
  std::printf("CONSUMER SMOKE TEST PASSED\n");
  return 0;
}