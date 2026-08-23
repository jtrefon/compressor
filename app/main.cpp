// app/main.cpp — CLI adapter for CompressionService.
//
// This is the outermost layer: parse arguments -> dispatch to the facade ->
// render results. It contains no compression, framing, or I/O logic — all of
// that lives in the library. The strategy list is generated from the
// CodecRegistry, so new algorithms appear here automatically.

#include <compression/FileFormat.hpp>
#include <compression/app/CompressionService.hpp>
#include <compression/codec/CodecRegistry.hpp>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

using namespace compression;

namespace {

struct CliOptions {
  std::string operation;
  std::string strategy = "default";
  std::filesystem::path input;
  std::filesystem::path output;
  std::size_t threads = 1; // default single-threaded (best ratio)
};

void printUsage(const char *appName) {
  std::cerr << "Usage: " << appName
            << " <compress|decompress> <strategy|ignored_on_decompress> "
               "<input_file> <output_file> [--threads N|--no-threads]\n"
            << "Strategies: default(optimized), ";
  bool first = true;
  for (const auto &[id, name] : codec::CodecRegistry::instance().all()) {
    // These are not user-selectable as CLI strategies.
    if (id == format::AlgorithmID::NULL_COMPRESSOR ||
        id == format::AlgorithmID::RLE_COMPRESSOR) {
      continue;
    }
    if (!first) {
      std::cerr << ", ";
    }
    first = false;
    std::cerr << name;
  }
  std::cerr << "\n";
}

// Parses arguments, printing usage on error. Returns nullopt to exit(1).
std::optional<CliOptions> parseArgs(int argc, char *argv[]) {
  if (argc < 5) {
    printUsage(argv[0]);
    return std::nullopt;
  }

  CliOptions cli;
  cli.operation = argv[1];
  cli.strategy = argv[2];
  cli.input = argv[3];
  cli.output = argv[4];

  if (argc >= 6) {
    const std::string arg = argv[5];
    if (arg == "--no-threads") {
      cli.threads = 1;
    } else if (arg == "--threads" && argc >= 7) {
      cli.threads = static_cast<std::size_t>(std::stoul(argv[6]));
    } else if (arg.rfind("--threads=", 0) == 0) {
      cli.threads = static_cast<std::size_t>(std::stoul(arg.substr(10)));
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
      printUsage(argv[0]);
      return std::nullopt;
    }
  }

  if (cli.operation != "compress" && cli.operation != "decompress") {
    std::cerr << "Error: Invalid operation. Must be 'compress' or "
                 "'decompress'.\n";
    printUsage(argv[0]);
    return std::nullopt;
  }
  return cli;
}

// CLI policy: "default" selects Optimized; Null/RLE are not selectable.
format::AlgorithmID resolveStrategy(const std::string &name) {
  if (name == "default") {
    return format::AlgorithmID::OPTIMIZED_COMPRESSOR;
  }
  if (!codec::CodecRegistry::instance().contains(name)) {
    throw std::invalid_argument("Unknown compression strategy: " + name);
  }
  const format::AlgorithmID id = codec::CodecRegistry::instance().idOf(name);
  if (id == format::AlgorithmID::NULL_COMPRESSOR ||
      id == format::AlgorithmID::RLE_COMPRESSOR) {
    throw std::invalid_argument("Strategy is disabled: " + name);
  }
  return id;
}

// Event sink: renders progress events from the facade to the console.
class ProgressPrinter final : public events::IEventListener {
public:
  void onEvent(const events::CompressionEvent &event) override {
    switch (event.type) {
    case events::EventType::OperationStarted:
      std::cout << "  Progress: 0%" << std::flush;
      break;
    case events::EventType::ChunkProgress:
      std::cout << "\r  Progress: " << static_cast<int>(event.progressPct)
                << "%" << std::flush;
      break;
    case events::EventType::OperationCompleted:
      std::cout << "\r  Progress: " << static_cast<int>(event.progressPct)
                << "%" << std::endl;
      break;
    default:
      break;
    }
  }
};

void printCompressSummary(const CompressResult &result) {
  std::cout << "  input  : " << result.inBytes << " bytes\n"
            << "  output : " << result.outBytes << " bytes (ratio "
            << std::fixed << std::setprecision(2) << (result.ratio * 100.0)
            << "%)\n"
            << "  crc32  : 0x" << std::hex << std::setw(8) << std::setfill('0')
            << result.crc << std::dec << std::setfill(' ') << "\n"
            << "  elapsed: " << result.elapsed.count() << " ms\n";
}

void printExtractSummary(const ExtractResult &result) {
  std::cout << "  input  : " << result.inBytes << " bytes\n"
            << "  output : " << result.outBytes << " bytes\n"
            << "  crc32  : 0x" << std::hex << std::setw(8) << std::setfill('0')
            << result.crc << std::dec << std::setfill(' ') << " (verified)\n"
            << "  elapsed: " << result.elapsed.count() << " ms\n";
}

} // namespace

int main(int argc, char *argv[]) {
  const std::optional<CliOptions> parsed = parseArgs(argc, argv);
  if (!parsed) {
    return 1;
  }
  const CliOptions &cli = *parsed;

  try {
    auto bus = std::make_shared<events::EventBus>();
    auto progress = std::make_shared<ProgressPrinter>();
    bus->subscribe(progress);
    CompressionService service(bus);

    if (cli.operation == "compress") {
      CompressionOptions options;
      options.codec = resolveStrategy(cli.strategy);
      options.threads = cli.threads;

      std::cout << "Compressing " << cli.input.string() << " -> "
                << cli.output.string() << " [" << cli.strategy << "]\n";
      const CompressResult result =
          service.compressFile(cli.input, cli.output, options);
      printCompressSummary(result);
    } else {
      std::cout << "Decompressing " << cli.input.string() << " -> "
                << cli.output.string() << "\n";
      const ExtractResult result =
          service.decompressFile(cli.input, cli.output);
      printExtractSummary(result);
    }

    std::cout << cli.operation << " completed successfully." << std::endl;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
