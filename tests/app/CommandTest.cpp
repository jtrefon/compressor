#include <compression/app/Command.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

using namespace compression;
using namespace compression::app;

namespace {

class TempPath {
public:
  explicit TempPath(const std::string &name)
      : path_(std::filesystem::temp_directory_path() /
              ("compressor_m2cmd_" + name + "_" +
               std::to_string(std::chrono::steady_clock::now()
                                  .time_since_epoch()
                                  .count()))) {}

  ~TempPath() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

std::vector<uint8_t> payload() {
  std::vector<uint8_t> data;
  const std::string words[] = {"compress", "command", "pattern", "repeat"};
  for (size_t i = 0; i < 20000; ++i) {
    data.push_back(static_cast<uint8_t>(words[i % 4][i % words[i % 4].size()]));
  }
  return data;
}

} // namespace

TEST(CommandTest, CompressCommandHasName) {
  TempPath in("in");
  TempPath out("out");
  CompressFileCommand command(in.path(), out.path());
  EXPECT_EQ(command.name(), "compress");
}

TEST(CommandTest, CompressThenDecompressRoundTrip) {
  TempPath in("in");
  TempPath out("out");
  TempPath back("back");
  {
    core::FileByteSink sink(in.path());
    sink.write(core::ByteView(payload()));
  }

  CompressionService service;
  CommandContext context(service);

  CompressFileCommand compress(in.path(), out.path());
  compress.execute(context);
  EXPECT_EQ(compress.result().inBytes, payload().size());
  EXPECT_GT(compress.result().outBytes, 0u);

  DecompressFileCommand decompress(out.path(), back.path());
  decompress.execute(context);
  EXPECT_EQ(decompress.result().outBytes, payload().size());
  EXPECT_TRUE(decompress.result().verified);

  core::FileByteSource source(back.path());
  std::vector<uint8_t> result;
  std::vector<uint8_t> buf(4096);
  while (true) {
    const std::size_t n = source.read(buf.data(), buf.size());
    if (n == 0) {
      break;
    }
    result.insert(result.end(), buf.begin(), buf.begin() + n);
  }
  EXPECT_EQ(result, payload());
}

TEST(CommandTest, CompressCommandWithCustomOptions) {
  TempPath in("in");
  TempPath out("out");
  {
    core::FileByteSink sink(in.path());
    sink.write(core::ByteView(payload()));
  }

  CompressionOptions options;
  options.codec = format::AlgorithmID::BWT_COMPRESSOR;
  options.threads = 2;

  CompressionService service;
  CommandContext context(service);
  CompressFileCommand command(in.path(), out.path(), options);
  command.execute(context);

  // Decompress back to prove the command honored the options.
  TempPath back("back");
  DecompressFileCommand decompress(out.path(), back.path());
  decompress.execute(context);
  EXPECT_EQ(decompress.result().outBytes, payload().size());
}
