#include <compression/codec/CodecRegistry.hpp>
#include <compression/codec/ParallelCodecDecorator.hpp>
#include <compression/core/Executor.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace compression;
using namespace compression::codec;
using namespace compression::core;

namespace {

// Deterministic xorshift32 so failures reproduce exactly (no <random> state
// to pin down, no libFuzzer dependency — CI-stable mutation fuzz).
class SeededRng {
public:
  explicit SeededRng(uint32_t seed) : state_(seed == 0 ? 0x12345678u : seed) {}

  uint32_t next() {
    uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = x;
    return x;
  }

  size_t below(size_t bound) { return bound == 0 ? 0 : next() % bound; }

  uint8_t byte() { return static_cast<uint8_t>(next()); }

private:
  uint32_t state_;
};

// A varied corpus: empty, plain text, repetitive, high-entropy random and
// binary-ish data. Deterministic (no <random>).
std::vector<std::vector<uint8_t>> corpus() {
  std::vector<std::vector<uint8_t>> inputs;

  inputs.emplace_back();

  std::vector<uint8_t> text;
  for (size_t i = 0; i < 512; ++i) {
    text.push_back(static_cast<uint8_t>('a' + (i * 7 + i / 13) % 26));
    if (i % 17 == 0) text.push_back(' ');
  }
  inputs.push_back(text);

  std::vector<uint8_t> repetitive;
  for (size_t i = 0; i < 2000; ++i) {
    repetitive.push_back(static_cast<uint8_t>('A' + (i % 3)));
  }
  inputs.push_back(repetitive);

  SeededRng rng(0xC0DEC0DEu);
  std::vector<uint8_t> randomBytes;
  for (size_t i = 0; i < 1024; ++i) {
    randomBytes.push_back(rng.byte());
  }
  inputs.push_back(randomBytes);

  std::vector<uint8_t> binary;
  for (size_t i = 0; i < 500; ++i) {
    binary.push_back(static_cast<uint8_t>((i * 97 + i * i * 13) & 0xFF));
    if (i % 64 == 0) binary.push_back(0x00);
    if (i % 64 == 32) binary.push_back(0xFF);
  }
  inputs.push_back(binary);

  return inputs;
}

// Corrupted input may only surface as std::runtime_error (the documented
// contract — all library errors derive from it). Anything else is a bug.
void expectOnlyRuntimeErrors(ICompressor &codec,
                             const std::vector<uint8_t> &mutated,
                             const std::string &context) {
  try {
    (void)codec.decompress(mutated);
  } catch (const std::runtime_error &) {
    // Expected rejection of corrupted input.
  } catch (const std::exception &e) {
    FAIL() << context << ": non-runtime_error exception on corrupted input: "
           << e.what();
  } catch (...) {
    FAIL() << context << ": unknown exception type on corrupted input";
  }
}

} // namespace

TEST(FuzzGatesTest, AllRegistryCodecsSurviveDeterministicMutationFuzz) {
  const auto &registry = CodecRegistry::instance();
  const auto all = registry.all();
  ASSERT_GE(all.size(), 9u);
  const auto inputs = corpus();

  for (const auto &[id, name] : all) {
    auto codec = registry.create(id);
    for (const auto &input : inputs) {
      const std::vector<uint8_t> encoded = codec->compress(input);
      EXPECT_EQ(codec->decompress(encoded), input)
          << "sanity round-trip failed for " << name;
      if (HasFatalFailure()) {
        return;
      }

      SeededRng rng(static_cast<uint32_t>(0xFU + 1013u * static_cast<unsigned>(id)));
      for (int iter = 0; iter < 150; ++iter) {
        std::vector<uint8_t> mutated = encoded;
        switch (rng.below(5)) {
        case 0: // flip one byte
          if (!mutated.empty()) {
            mutated[rng.below(mutated.size())] ^=
                static_cast<uint8_t>(1u << rng.below(8));
          }
          break;
        case 1: // truncate
          if (!mutated.empty()) {
            mutated.resize(rng.below(mutated.size() + 1));
          }
          break;
        case 2: // append junk
          for (size_t n = rng.below(16); n > 0; --n) {
            mutated.push_back(rng.byte());
          }
          break;
        case 3: // replace with a small random buffer
          mutated.clear();
          for (size_t n = rng.below(64); n > 0; --n) {
            mutated.push_back(rng.byte());
          }
          break;
        default: { // flip a byte near the frame header (first 32 bytes)
          const size_t pos = rng.below(32);
          if (pos < mutated.size()) {
            mutated[pos] ^= 0xFF;
          }
        } break;
        }
        expectOnlyRuntimeErrors(*codec, mutated, name);
      }
    }
  }
}

TEST(FuzzGatesTest, ParallelCodecDecoratorSurvivesDeterministicMutationFuzz) {
  InlineExecutor executor;
  const auto &registry = CodecRegistry::instance();
  const auto inputs = corpus();

  for (const auto &[id, name] : registry.all()) {
    auto codec = std::make_unique<ParallelCodecDecorator>(
        registry.create(id), id, &executor, 3);
    for (const auto &input : inputs) {
      const std::vector<uint8_t> encoded = codec->compress(input);
      EXPECT_EQ(codec->decompress(encoded), input)
          << "sanity round-trip failed for framed " << name;
      if (HasFatalFailure()) {
        return;
      }

      SeededRng rng(static_cast<uint32_t>(0xA5A5u + 37u * static_cast<unsigned>(id)));
      for (int iter = 0; iter < 100; ++iter) {
        std::vector<uint8_t> mutated = encoded;
        switch (rng.below(5)) {
        case 0:
          if (!mutated.empty()) {
            mutated[rng.below(mutated.size())] ^=
                static_cast<uint8_t>(1u << rng.below(8));
          }
          break;
        case 1:
          if (!mutated.empty()) {
            mutated.resize(rng.below(mutated.size() + 1));
          }
          break;
        case 2:
          for (size_t n = rng.below(16); n > 0; --n) {
            mutated.push_back(rng.byte());
          }
          break;
        case 3:
          mutated.clear();
          for (size_t n = rng.below(64); n > 0; --n) {
            mutated.push_back(rng.byte());
          }
          break;
        default: { // corrupt the CPRO header (magic/version/algo/sizes)
          const size_t pos = rng.below(30);
          if (pos < mutated.size()) {
            mutated[pos] = static_cast<uint8_t>(rng.byte());
          }
        } break;
        }
        expectOnlyRuntimeErrors(*codec, mutated, "framed " + name);
      }
    }
  }
}