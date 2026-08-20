#pragma once

#include <compression/codec/pipeline/Pipeline.hpp>

#include <cstdint>
#include <vector>

namespace compression {
namespace codec {
namespace pipeline {

/**
 * @brief Static rANS entropy coder (32-bit state, byte renormalization).
 *
 * The emitted payload is self-describing:
 *   [256 x u16 LE normalized frequencies (table sum = 2^14)]
 *   [u32 LE symbol count]
 *   [renormalization bytes, consumed backwards by the decoder]
 *   [u32 LE final encoder state]
 *
 * Every encoded symbol emits at least one renorm byte, so a hostile symbol
 * count can never drive more decode work than the payload size allows.
 */
class AnsCoder final : public IEntropyCoder {
public:
  uint8_t id() const override { return kCoderAns; }
  std::vector<uint8_t> encode(core::ByteView data) const override;
  std::vector<uint8_t> decode(core::ByteView data) const override;
};

} // namespace pipeline
} // namespace codec
} // namespace compression