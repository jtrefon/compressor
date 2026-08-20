#pragma once

#include <compression/core/BinaryIO.hpp>
#include <compression/FileFormat.hpp>

#include <memory>
#include <vector>

namespace compression {
namespace format {

/**
 * @brief Magic of the 1.x raw container ("CPRO").
 */
inline constexpr core::FourCC LEGACY_FRAME_MAGIC =
    core::makeFourCC('C', 'P', 'R', 'O');

/**
 * @brief A container framing codec: encodes/decodes a container header.
 *
 * The payload following the header is opaque to the frame codec; the caller
 * slices it using the chunk metadata in FileHeader.
 */
class IFrameCodec {
public:
  virtual ~IFrameCodec() = default;

  virtual core::FourCC magic() const = 0;

  virtual std::vector<uint8_t> encodeHeader(const FileHeader &header) const = 0;

  /**
   * @brief Parses the header from the front of a byte stream.
   * @throws InvalidFormatError on bad magic, UnsupportedVersionError on
   * unknown version, CorruptDataError on truncated/malformed data.
   */
  virtual FileHeader decodeHeader(core::ByteView stream) const = 0;
};

/**
 * @brief Registry of container framing codecs, keyed by magic.
 *
 * Format evolution = registering a new frame codec with a new magic or
 * version; old formats stay readable via their registered codec.
 */
class FrameRegistry {
public:
  static FrameRegistry &instance();

  void registerCodec(std::unique_ptr<IFrameCodec> codec);

  /**
   * @brief Looks up a frame codec by magic; nullptr if absent.
   */
  const IFrameCodec *find(core::FourCC magic) const;

  /**
   * @brief Like find(), but throws InvalidFormatError if absent.
   */
  const IFrameCodec *require(core::FourCC magic) const;

private:
  FrameRegistry() = default;

  std::vector<std::unique_ptr<IFrameCodec>> codecs_;
};

} // namespace format
} // namespace compression
