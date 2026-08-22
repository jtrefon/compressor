#pragma once

#include <compression/core/ByteSource.hpp>
#include <compression/core/Errors.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace compression {
namespace core {

/**
 * @brief Four-character code identifying a format or frame.
 */
struct FourCC {
  uint32_t value;

  constexpr bool operator==(FourCC other) const { return value == other.value; }
  constexpr bool operator!=(FourCC other) const { return value != other.value; }
};

/**
 * @brief Builds a FourCC from four ASCII characters.
 */
inline constexpr FourCC makeFourCC(char a, char b, char c, char d) {
  return FourCC{static_cast<uint32_t>(static_cast<uint8_t>(a)) |
                (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8) |
                (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16) |
                (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24)};
}

namespace detail {
inline std::string toHex(uint32_t value) {
  const char *digits = "0123456789ABCDEF";
  std::string s(8, '0');
  for (int i = 7; i >= 0; --i) {
    s[i] = digits[value & 0xF];
    value >>= 4;
  }
  return s;
}
} // namespace detail

/**
 * @brief Endian-safe binary writer (Adapter over IByteSink).
 *
 * All integers are written explicitly little-endian, so streams are
 * byte-identical across platforms.
 */
class BinaryWriter {
public:
  explicit BinaryWriter(IByteSink &sink) : sink_(sink) {}

  void u8(uint8_t value) { sink_.writeByte(value); }

  void u16(uint16_t value) {
    sink_.writeByte(static_cast<uint8_t>(value & 0xFF));
    sink_.writeByte(static_cast<uint8_t>((value >> 8) & 0xFF));
  }

  void u32(uint32_t value) {
    sink_.writeByte(static_cast<uint8_t>(value & 0xFF));
    sink_.writeByte(static_cast<uint8_t>((value >> 8) & 0xFF));
    sink_.writeByte(static_cast<uint8_t>((value >> 16) & 0xFF));
    sink_.writeByte(static_cast<uint8_t>((value >> 24) & 0xFF));
  }

  void u64(uint64_t value) {
    for (int i = 0; i < 8; ++i) {
      sink_.writeByte(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
    }
  }

  void bytes(ByteView data) { sink_.write(data); }

  void magic(FourCC m) { u32(m.value); }

  uint64_t size() const { return sink_.size(); }

private:
  IByteSink &sink_;
};

/**
 * @brief Endian-safe, bounds-checked binary reader (Adapter over IByteSource).
 *
 * Throws CorruptDataError on short reads and InvalidFormatError on magic
 * mismatch.
 */
class BinaryReader {
public:
  explicit BinaryReader(IByteSource &src) : src_(src) {}

  uint8_t u8() {
    uint8_t b = 0;
    readExact(&b, 1);
    return b;
  }

  uint16_t u16() {
    uint16_t value = 0;
    for (int i = 0; i < 2; ++i) {
      value |= static_cast<uint16_t>(u8()) << (8 * i);
    }
    return value;
  }

  uint32_t u32() {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
      value |= static_cast<uint32_t>(u8()) << (8 * i);
    }
    return value;
  }

  uint64_t u64() {
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
      value |= static_cast<uint64_t>(u8()) << (8 * i);
    }
    return value;
  }

  /**
   * @brief Reads exactly @p count bytes; throws CorruptDataError if the
   * stream ends early.
   */
  void readExact(uint8_t *dst, std::size_t count) {
    std::size_t got = 0;
    while (got < count) {
      const std::size_t n = src_.read(dst + got, count - got);
      if (n == 0) {
        throw CorruptDataError("Unexpected end of data: truncated stream");
      }
      got += n;
    }
  }

  /**
   * @brief Reads @p count bytes into an internal scratch buffer and returns
   * a view. The view is valid until the next readBytes/skip call.
   */
  ByteView readBytes(std::size_t count) {
    if (count > scratch_.size()) {
      scratch_.resize(count);
    }
    readExact(scratch_.data(), count);
    return ByteView(scratch_.data(), count);
  }

  /**
   * @brief Reads and verifies a magic; throws InvalidFormatError on mismatch.
   */
  void expectMagic(FourCC expected) {
    const uint32_t got = u32();
    if (got != expected.value) {
      throw InvalidFormatError("Invalid magic: expected 0x" +
                               detail::toHex(expected.value) + ", got 0x" +
                               detail::toHex(got));
    }
  }

  /**
   * @brief Reads a magic and reports whether it matched.
   * Bytes are consumed either way.
   */
  bool tryMagic(FourCC expected) { return u32() == expected.value; }

  /**
   * @brief Skips @p count bytes forward.
   */
  void skip(std::size_t count) { readBytes(count); }

  uint64_t position() const { return src_.position(); }
  uint64_t remaining() const { return src_.size() - src_.position(); }

private:
  IByteSource &src_;
  std::vector<uint8_t> scratch_;
};

} // namespace core
} // namespace compression
