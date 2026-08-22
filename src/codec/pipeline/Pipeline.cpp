#include <compression/codec/pipeline/AnsCoder.hpp>
#include <compression/codec/pipeline/Pipeline.hpp>

#include <compression/ArithmeticCompressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <compression/core/BinaryIO.hpp>
#include <compression/core/Errors.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace compression {
namespace codec {
namespace pipeline {

namespace {

constexpr core::FourCC kPipelineMagic = core::makeFourCC('P', 'L', 'I', 'N');
constexpr uint8_t kPipelineVersion = 1;
constexpr uint8_t kMaxStages = 16;

std::vector<uint8_t> toVector(core::ByteView view) {
  return std::vector<uint8_t>(view.begin(), view.end());
}

} // namespace

// --- RleTransform ---
//
// Self-describing token stream: each token starts with a type byte.
//   0x00 [len u8] [value]        -> repeat `value` `len` times
//   0x01 [len u8] [len bytes]    -> literal run of `len` bytes
// No byte value needs escaping; the format is unambiguous.

std::vector<uint8_t> RleTransform::forward(core::ByteView data) const {
  std::vector<uint8_t> out;
  if (data.empty()) {
    return out;
  }
  std::size_t i = 0;
  while (i < data.size()) {
    std::size_t run = 1;
    while (i + run < data.size() && data[i + run] == data[i] &&
           run < 255) {
      ++run;
    }
    if (run >= 3) {
      out.push_back(0x00);
      out.push_back(static_cast<uint8_t>(run));
      out.push_back(data[i]);
      i += run;
    } else {
      const std::size_t start = i;
      std::size_t literalLen = 0;
      while (i + literalLen < data.size()) {
        std::size_t nextRun = 1;
        while (i + literalLen + nextRun < data.size() &&
               data[i + literalLen + nextRun] == data[i + literalLen] &&
               nextRun < 255) {
          ++nextRun;
        }
        if (nextRun >= 3) {
          break;
        }
        ++literalLen;
        if (literalLen == 255) {
          break;
        }
      }
      out.push_back(0x01);
      out.push_back(static_cast<uint8_t>(literalLen));
      for (std::size_t k = 0; k < literalLen; ++k) {
        out.push_back(data[start + k]);
      }
      i = start + literalLen;
    }
  }
  return out;
}

std::vector<uint8_t> RleTransform::inverse(core::ByteView data) const {
  std::vector<uint8_t> out;
  std::size_t i = 0;
  while (i < data.size()) {
    if (i + 2 > data.size()) {
      throw core::CorruptDataError("Truncated RLE token");
    }
    const uint8_t type = data[i];
    const uint8_t len = data[i + 1];
    if (type == 0x00) {
      if (i + 3 > data.size()) {
        throw core::CorruptDataError("Truncated RLE run token");
      }
      out.insert(out.end(), len, data[i + 2]);
      i += 3;
    } else if (type == 0x01) {
      if (i + 2 + len > data.size()) {
        throw core::CorruptDataError("Truncated RLE literal token");
      }
      out.insert(out.end(), data.begin() + i + 2,
                 data.begin() + i + 2 + len);
      i += 2 + len;
    } else {
      throw core::CorruptDataError("Unknown RLE token type");
    }
  }
  return out;
}

// --- BwtTransformAdapter ---

std::vector<uint8_t> BwtTransformAdapter::forward(core::ByteView data) const {
  BwtCompressor bwt;
  return bwt.transform(toVector(data));
}

std::vector<uint8_t> BwtTransformAdapter::inverse(core::ByteView data) const {
  BwtCompressor bwt;
  return bwt.inverseTransform(toVector(data));
}

// --- ArithmeticCoderAdapter ---

std::vector<uint8_t> ArithmeticCoderAdapter::encode(core::ByteView data) const {
  ArithmeticCompressor arithmetic;
  return arithmetic.compress(toVector(data));
}

std::vector<uint8_t> ArithmeticCoderAdapter::decode(core::ByteView data) const {
  ArithmeticCompressor arithmetic;
  return arithmetic.decompress(toVector(data));
}

// --- Stage factory ---

std::unique_ptr<ITransform> createTransform(uint8_t id) {
  switch (id) {
  case kTransformBwt:
    return std::make_unique<BwtTransformAdapter>();
  case kTransformRle:
    return std::make_unique<RleTransform>();
  default:
    throw core::ConfigurationError("Unknown pipeline transform id: " +
                                   std::to_string(id));
  }
}

std::unique_ptr<IEntropyCoder> createCoder(uint8_t id) {
  switch (id) {
  case kCoderArithmetic:
    return std::make_unique<ArithmeticCoderAdapter>();
  case kCoderAns:
    return std::make_unique<AnsCoder>();
  default:
    throw core::ConfigurationError("Unknown pipeline coder id: " +
                                   std::to_string(id));
  }
}

// --- PipelineCodec ---

PipelineCodec::PipelineCodec(std::vector<std::unique_ptr<ITransform>> transforms,
                             std::unique_ptr<IEntropyCoder> coder)
    : transforms_(std::move(transforms)), coder_(std::move(coder)) {
  if (!coder_) {
    throw core::ConfigurationError("PipelineCodec requires a coder");
  }
}

std::vector<uint8_t>
PipelineCodec::compress(const std::vector<uint8_t> &data) const {
  std::vector<uint8_t> current = data;
  for (const auto &transform : transforms_) {
    current = transform->forward(core::ByteView(current));
  }
  std::vector<uint8_t> payload = coder_->encode(core::ByteView(current));

  core::MemoryByteSink sink;
  core::BinaryWriter writer(sink);
  writer.magic(kPipelineMagic);
  writer.u8(kPipelineVersion);
  writer.u8(static_cast<uint8_t>(transforms_.size()));
  for (const auto &transform : transforms_) {
    writer.u8(transform->id());
  }
  writer.u8(coder_->id());
  writer.bytes(core::ByteView(payload));
  return sink.data();
}

std::vector<uint8_t>
PipelineCodec::decompress(const std::vector<uint8_t> &data) const {
  core::MemoryByteSource source(data);
  core::BinaryReader reader(source);
  reader.expectMagic(kPipelineMagic);
  const uint8_t version = reader.u8();
  if (version != kPipelineVersion) {
    throw core::UnsupportedVersionError(
        "Unsupported pipeline version: " + std::to_string(version));
  }
  const uint8_t stageCount = reader.u8();
  if (stageCount > kMaxStages) {
    throw core::CorruptDataError("Pipeline stage count too large");
  }
  std::vector<uint8_t> transformIds(stageCount);
  for (uint8_t &id : transformIds) {
    id = reader.u8();
  }
  const uint8_t coderId = reader.u8();
  const core::ByteView payload = reader.readBytes(reader.remaining());

  std::vector<uint8_t> current = toVector(payload);
  current = createCoder(coderId)->decode(core::ByteView(current));
  for (auto it = transformIds.rbegin(); it != transformIds.rend(); ++it) {
    current = createTransform(*it)->inverse(core::ByteView(current));
  }
  return current;
}

} // namespace pipeline
} // namespace codec
} // namespace compression
