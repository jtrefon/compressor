#include <compression/FileFormat.hpp>
#include <compression/format/FrameRegistry.hpp>

namespace compression {
namespace format {

std::vector<uint8_t> serializeHeader(const FileHeader &header) {
  const IFrameCodec *codec =
      FrameRegistry::instance().require(LEGACY_FRAME_MAGIC);
  return codec->encodeHeader(header);
}

FileHeader deserializeHeader(const std::vector<uint8_t> &buffer) {
  const IFrameCodec *codec =
      FrameRegistry::instance().require(LEGACY_FRAME_MAGIC);
  return codec->decodeHeader(core::ByteView(buffer));
}

} // namespace format
} // namespace compression
