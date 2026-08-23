#include <compression/codec/CodecRegistry.hpp>

#include <compression/codec/legacy/ArithmeticCompressor.hpp>
#include <compression/codec/legacy/BwtCompressor.hpp>
#include <compression/codec/legacy/ExtremeCompressor.hpp>
#include <compression/codec/legacy/HuffmanCompressor.hpp>
#include <compression/codec/legacy/Lz77Compressor.hpp>
#include <compression/codec/legacy/NullCompressor.hpp>
#include <compression/codec/legacy/OptimizedCompressor.hpp>
#include <compression/codec/legacy/RleCompressor.hpp>
#include <compression/codec/legacy/UltraCompressor.hpp>
#include <compression/codec/pipeline/AnsCoder.hpp>
#include <compression/codec/pipeline/Pipeline.hpp>
#include <compression/core/Errors.hpp>

#include <stdexcept>

namespace compression {
namespace codec {

namespace {

// Add a new algorithm here: include its header above and register it below.
// Nothing else in the codebase needs to change (Open/Closed Principle).
void registerBuiltinCodecs(CodecRegistry &registry) {
  registry.registerCodec(format::AlgorithmID::NULL_COMPRESSOR, "null",
                         []() -> std::unique_ptr<ICompressor> { return std::make_unique<NullCompressor>(); });
  registry.registerCodec(format::AlgorithmID::RLE_COMPRESSOR, "rle",
                         []() -> std::unique_ptr<ICompressor> { return std::make_unique<RleCompressor>(); });
  registry.registerCodec(format::AlgorithmID::HUFFMAN_COMPRESSOR, "huffman",
                         []() -> std::unique_ptr<ICompressor> { return std::make_unique<HuffmanCompressor>(); });
  registry.registerCodec(
      format::AlgorithmID::LZ77_COMPRESSOR, "lz77",
      []() -> std::unique_ptr<ICompressor> {
        // Canonical tuned LZ77 configuration (was duplicated in two factories).
        return std::make_unique<Lz77Compressor>(32768, 3, 258, false, true,
                                                true);
      });
  registry.registerCodec(format::AlgorithmID::BWT_COMPRESSOR, "bwt",
                         []() -> std::unique_ptr<ICompressor> { return std::make_unique<BwtCompressor>(); });
  registry.registerCodec(format::AlgorithmID::ULTRA_COMPRESSOR, "ultra",
                         []() -> std::unique_ptr<ICompressor> { return std::make_unique<UltraCompressor>(); });
  registry.registerCodec(format::AlgorithmID::EXTREME_COMPRESSOR, "extreme",
                         []() -> std::unique_ptr<ICompressor> { return std::make_unique<ExtremeCompressor>(); });
  registry.registerCodec(format::AlgorithmID::OPTIMIZED_COMPRESSOR,
                         "optimized",
                         []() -> std::unique_ptr<ICompressor> { return std::make_unique<OptimizedCompressor>(); });
  registry.registerCodec(format::AlgorithmID::ARITHMETIC_COMPRESSOR,
                         "arithmetic",
                         []() -> std::unique_ptr<ICompressor> { return std::make_unique<ArithmeticCompressor>(); });
  // Pipeline codec (Open/Closed proof: a new codec added purely via
  // registration — the CLI/UI discover it from the registry).
  registry.registerCodec(
      format::AlgorithmID::PIPELINE_BWT_COMPRESSOR, "bwt2",
      []() -> std::unique_ptr<ICompressor> {
        using namespace codec::pipeline;
        std::vector<std::unique_ptr<ITransform>> transforms;
        transforms.push_back(std::make_unique<BwtTransformAdapter>());
        transforms.push_back(std::make_unique<RleTransform>());
        return std::make_unique<PipelineCodec>(
            std::move(transforms),
            std::make_unique<ArithmeticCoderAdapter>());
      });
  // Second Open/Closed proof: a brand-new entropy coder (static rANS) shipped
  // purely as a new file + registration; zero edits to factories/CLI/UI.
  registry.registerCodec(
      format::AlgorithmID::ANS_COMPRESSOR, "ans",
      []() -> std::unique_ptr<ICompressor> {
        using namespace codec::pipeline;
        return std::make_unique<PipelineCodec>(
            std::vector<std::unique_ptr<ITransform>>{},
            std::make_unique<AnsCoder>());
      });
}

} // namespace

CodecRegistry &CodecRegistry::instance() {
  // Function-local static: constructed once on first use (thread-safe in
  // C++11+), before any client can observe it. Contents are fixed thereafter.
  static CodecRegistry *registry = []() {
    auto *r = new CodecRegistry();
    registerBuiltinCodecs(*r);
    return r;
  }();
  return *registry;
}

void CodecRegistry::registerCodec(format::AlgorithmID id, std::string_view name,
                                  Factory factory) {
  if (byId_.count(id) != 0) {
    throw std::logic_error("Codec already registered for id " +
                           std::to_string(static_cast<uint8_t>(id)));
  }
  if (byName_.count(std::string(name)) != 0) {
    throw std::logic_error("Codec already registered for name: " +
                           std::string(name));
  }
  byId_.emplace(id, Entry{std::string(name), factory});
  byName_.emplace(std::string(name), id);
}

bool CodecRegistry::contains(format::AlgorithmID id) const {
  return byId_.count(id) != 0;
}

bool CodecRegistry::contains(std::string_view name) const {
  return byName_.count(std::string(name)) != 0;
}

format::AlgorithmID CodecRegistry::idOf(std::string_view name) const {
  const auto it = byName_.find(std::string(name));
  return it == byName_.end() ? format::AlgorithmID::UNKNOWN : it->second;
}

std::string_view CodecRegistry::nameOf(format::AlgorithmID id) const {
  const auto it = byId_.find(id);
  if (it == byId_.end()) {
    return "unknown";
  }
  return it->second.name;
}

std::unique_ptr<ICompressor> CodecRegistry::create(format::AlgorithmID id) const {
  const auto it = byId_.find(id);
  if (it == byId_.end()) {
    throw core::ConfigurationError("Unknown codec id: " +
                                   std::to_string(static_cast<uint8_t>(id)));
  }
  return it->second.factory();
}

std::unique_ptr<ICompressor> CodecRegistry::create(std::string_view name) const {
  const format::AlgorithmID id = idOf(name);
  if (id == format::AlgorithmID::UNKNOWN) {
    throw core::ConfigurationError("Unknown codec: '" + std::string(name) +
                                   "'");
  }
  return create(id);
}

std::vector<std::pair<format::AlgorithmID, std::string>>
CodecRegistry::all() const {
  std::vector<std::pair<format::AlgorithmID, std::string>> result;
  result.reserve(byId_.size());
  for (const auto &[id, entry] : byId_) {
    result.emplace_back(id, entry.name);
  }
  return result;
}

} // namespace codec
} // namespace compression
