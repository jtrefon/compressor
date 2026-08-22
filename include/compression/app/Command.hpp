#pragma once

#include <compression/app/CompressionService.hpp>

#include <filesystem>
#include <string>

namespace compression {
namespace app {

/**
 * @brief Gives commands access to the facade (and, later, other services).
 */
class CommandContext {
public:
  explicit CommandContext(CompressionService &service) : service_(&service) {}

  CompressionService &service() const { return *service_; }

private:
  CompressionService *service_;
};

/**
 * @brief A single bindable, repeatable operation (Command pattern).
 *
 * UI buttons and CLI verbs both map onto commands; commands carry their
 * own options and expose the last result for binding.
 */
class ICommand {
public:
  virtual ~ICommand() = default;

  virtual std::string name() const = 0;
  virtual void execute(const CommandContext &context) = 0;
};

/**
 * @brief Compresses one file via CompressionService.
 */
class CompressFileCommand final : public ICommand {
public:
  CompressFileCommand(std::filesystem::path in, std::filesystem::path out,
                      CompressionOptions options = {});

  std::string name() const override;
  void execute(const CommandContext &context) override;

  const CompressResult &result() const { return result_; }

private:
  std::filesystem::path in_;
  std::filesystem::path out_;
  CompressionOptions options_;
  CompressResult result_;
};

/**
 * @brief Decompresses one file via CompressionService.
 */
class DecompressFileCommand final : public ICommand {
public:
  DecompressFileCommand(std::filesystem::path in, std::filesystem::path out);

  std::string name() const override;
  void execute(const CommandContext &context) override;

  const ExtractResult &result() const { return result_; }

private:
  std::filesystem::path in_;
  std::filesystem::path out_;
  ExtractResult result_;
};

} // namespace app
} // namespace compression
