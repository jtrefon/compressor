#include <compression/app/Command.hpp>

#include <utility>

namespace compression {
namespace app {

CompressFileCommand::CompressFileCommand(std::filesystem::path in,
                                         std::filesystem::path out,
                                         CompressionOptions options)
    : in_(std::move(in)), out_(std::move(out)), options_(options) {}

std::string CompressFileCommand::name() const { return "compress"; }

void CompressFileCommand::execute(const CommandContext &context) {
  result_ = context.service().compressFile(in_, out_, options_);
}

DecompressFileCommand::DecompressFileCommand(std::filesystem::path in,
                                             std::filesystem::path out)
    : in_(std::move(in)), out_(std::move(out)) {}

std::string DecompressFileCommand::name() const { return "decompress"; }

void DecompressFileCommand::execute(const CommandContext &context) {
  result_ = context.service().decompressFile(in_, out_);
}

} // namespace app
} // namespace compression
