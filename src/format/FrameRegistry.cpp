#include <compression/format/FrameRegistry.hpp>

#include <compression/core/Errors.hpp>

#include <cstdint>
#include <string>

namespace compression {
namespace format {

namespace {

// Hostile-header hardening: a chunk count beyond this is always corrupt.
constexpr uint32_t kMaxChunks = 1024;

/**
 * @brief The 1.x raw container ("CPRO" magic + fixed header + payload).
 *
 * Byte layout is identical to the historic serializer: little-endian, no
 * padding. Reading validates magic, version, and chunk metadata with typed
 * errors and bounds checks.
 */
class LegacyV1FrameCodec : public IFrameCodec {
public:
  core::FourCC magic() const override { return LEGACY_FRAME_MAGIC; }

  std::vector<uint8_t> encodeHeader(const FileHeader &header) const override {
    core::MemoryByteSink sink;
    core::BinaryWriter writer(sink);
    writer.magic(LEGACY_FRAME_MAGIC);
    writer.u8(FORMAT_VERSION);
    writer.u8(static_cast<uint8_t>(header.algorithmId));
    writer.u64(header.originalSize);
    writer.u32(header.originalChecksum);
    writer.u32(header.chunkCount);
    writer.u32(header.chunkSize);
    for (uint32_t size : header.compressedSizes) {
      writer.u32(size);
    }
    return sink.data();
  }

  FileHeader decodeHeader(core::ByteView stream) const override {
    core::MemoryByteSource source(stream);
    core::BinaryReader reader(source);
    reader.expectMagic(LEGACY_FRAME_MAGIC);

    FileHeader header;
    header.formatVersion = reader.u8();
    if (header.formatVersion != FORMAT_VERSION) {
      throw core::UnsupportedVersionError(
          "Unsupported format version: " +
          std::to_string(header.formatVersion));
    }
    header.algorithmId = static_cast<AlgorithmID>(reader.u8());
    header.originalSize = reader.u64();
    header.originalChecksum = reader.u32();
    header.chunkCount = reader.u32();
    header.chunkSize = reader.u32();
    if (header.chunkCount == 0 || header.chunkCount > kMaxChunks) {
      throw core::CorruptDataError("Invalid chunk count in header");
    }
    header.compressedSizes.resize(header.chunkCount);
    for (uint32_t i = 0; i < header.chunkCount; ++i) {
      header.compressedSizes[i] = reader.u32();
    }
    return header;
  }
};

} // namespace

FrameRegistry &FrameRegistry::instance() {
  static FrameRegistry *registry = []() {
    auto *r = new FrameRegistry();
    r->registerCodec(std::make_unique<LegacyV1FrameCodec>());
    return r;
  }();
  return *registry;
}

void FrameRegistry::registerCodec(std::unique_ptr<IFrameCodec> codec) {
  codecs_.push_back(std::move(codec));
}

const IFrameCodec *FrameRegistry::find(core::FourCC magic) const {
  for (const auto &codec : codecs_) {
    if (codec->magic() == magic) {
      return codec.get();
    }
  }
  return nullptr;
}

const IFrameCodec *FrameRegistry::require(core::FourCC magic) const {
  const IFrameCodec *codec = find(magic);
  if (codec == nullptr) {
    throw core::InvalidFormatError("No frame codec registered for magic");
  }
  return codec;
}

} // namespace format
} // namespace compression
