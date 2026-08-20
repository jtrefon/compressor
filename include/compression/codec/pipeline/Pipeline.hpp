#pragma once

#include <compression/ICompressor.hpp>
#include <compression/core/ByteSource.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace compression {
namespace codec {
namespace pipeline {

/**
 * @brief A reversible transform stage (BWT, MTF, RLE, delta, BCJ...).
 *
 * Stages are stateless and thread-safe; a pipeline composes several in
 * forward order and their inverses in reverse order.
 */
class ITransform {
public:
  virtual ~ITransform() = default;
  virtual std::vector<uint8_t> forward(core::ByteView data) const = 0;
  virtual std::vector<uint8_t> inverse(core::ByteView data) const = 0;

  /**
   * @brief Stable stage id stored in the pipeline frame.
   */
  virtual uint8_t id() const = 0;
};

/**
 * @brief An entropy coder stage (Huffman, Arithmetic, ANS...).
 */
class IEntropyCoder {
public:
  virtual ~IEntropyCoder() = default;
  virtual std::vector<uint8_t> encode(core::ByteView data) const = 0;
  virtual std::vector<uint8_t> decode(core::ByteView data) const = 0;

  /**
   * @brief Stable stage id stored in the pipeline frame.
   */
  virtual uint8_t id() const = 0;
};

/**
 * @brief Composite codec: [ITransform...] -> IEntropyCoder, exposed as one
 * ICompressor (Composite + Template Method).
 *
 * The emitted container is a self-describing pipeline frame ("PLIN") so
 * decode rebuilds the exact stage graph from the byte stream.
 */
class PipelineCodec final : public ICompressor {
public:
  PipelineCodec(std::vector<std::unique_ptr<ITransform>> transforms,
                std::unique_ptr<IEntropyCoder> coder);

  std::vector<uint8_t> compress(const std::vector<uint8_t> &data) const override;
  std::vector<uint8_t> decompress(const std::vector<uint8_t> &data) const override;

private:
  std::vector<std::unique_ptr<ITransform>> transforms_;
  std::unique_ptr<IEntropyCoder> coder_;
};

// Stable stage ids stored in the pipeline frame.
constexpr uint8_t kTransformBwt = 1;    // raw BWT (legacy BwtCompressor transform)
constexpr uint8_t kTransformRle = 2;    // raw run-length transform
constexpr uint8_t kCoderArithmetic = 1; // adaptive order-1 arithmetic coder
constexpr uint8_t kCoderAns = 2;        // static rANS entropy coder

/**
 * @brief Creates a transform stage by id; throws ConfigurationError if
 * unknown.
 */
std::unique_ptr<ITransform> createTransform(uint8_t id);

/**
 * @brief Creates an entropy coder by id; throws ConfigurationError if
 * unknown.
 */
std::unique_ptr<IEntropyCoder> createCoder(uint8_t id);

/**
 * @brief Raw run-length transform stage.
 */
class RleTransform final : public ITransform {
public:
  uint8_t id() const override { return kTransformRle; }
  std::vector<uint8_t> forward(core::ByteView data) const override;
  std::vector<uint8_t> inverse(core::ByteView data) const override;
};

/**
 * @brief BWT stage, adapted from the legacy BwtCompressor.
 */
class BwtTransformAdapter final : public ITransform {
public:
  uint8_t id() const override { return kTransformBwt; }
  std::vector<uint8_t> forward(core::ByteView data) const override;
  std::vector<uint8_t> inverse(core::ByteView data) const override;
};

/**
 * @brief Arithmetic entropy coder, adapted from ArithmeticCompressor.
 */
class ArithmeticCoderAdapter final : public IEntropyCoder {
public:
  uint8_t id() const override { return kCoderArithmetic; }
  std::vector<uint8_t> encode(core::ByteView data) const override;
  std::vector<uint8_t> decode(core::ByteView data) const override;
};

} // namespace pipeline
} // namespace codec
} // namespace compression
