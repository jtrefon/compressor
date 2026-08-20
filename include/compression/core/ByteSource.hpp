#pragma once

#include <compression/core/ByteView.hpp>
#include <compression/core/Errors.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace compression {
namespace core {

/**
 * @brief Port: destination for byte streams (write side).
 *
 * Domain code writes through this interface; infrastructure provides
 * in-memory and file-backed implementations.
 */
class IByteSink {
public:
  virtual ~IByteSink() = default;

  virtual void write(ByteView data) = 0;
  virtual uint64_t size() const = 0;

  void writeByte(uint8_t byte) {
    const uint8_t buf[1] = {byte};
    write(ByteView(buf, 1));
  }
};

/**
 * @brief Port: source of byte streams (read side).
 */
class IByteSource {
public:
  virtual ~IByteSource() = default;

  virtual uint64_t size() const = 0;
  virtual uint64_t position() = 0;

  /**
   * @brief Moves the read cursor. Throws std::out_of_range if beyond end.
   */
  virtual void seek(uint64_t position) = 0;

  /**
   * @brief Reads up to @p count bytes into @p dst.
   * @return bytes read; 0 signals end of stream.
   */
  virtual std::size_t read(uint8_t *dst, std::size_t count) = 0;
};

/**
 * @brief In-memory sink owning a std::vector<uint8_t>.
 */
class MemoryByteSink : public IByteSink {
public:
  void write(ByteView data) override;
  uint64_t size() const override { return data_.size(); }

  const std::vector<uint8_t> &data() const { return data_; }
  std::vector<uint8_t> take() { return std::move(data_); }

private:
  std::vector<uint8_t> data_;
};

/**
 * @brief Non-owning in-memory source over a ByteView.
 *
 * The referenced buffer must outlive the source.
 */
class MemoryByteSource : public IByteSource {
public:
  explicit MemoryByteSource(ByteView data) : data_(data) {}

  uint64_t size() const override { return data_.size(); }
  uint64_t position() override { return position_; }
  void seek(uint64_t position) override;
  std::size_t read(uint8_t *dst, std::size_t count) override;

private:
  ByteView data_;
  uint64_t position_ = 0;
};

/**
 * @brief File-backed sink (infrastructure adapter).
 * Throws IoError on open/write failures.
 */
class FileByteSink : public IByteSink {
public:
  explicit FileByteSink(const std::filesystem::path &path);
  ~FileByteSink() override = default;

  void write(ByteView data) override;
  uint64_t size() const override { return written_; }

private:
  std::ofstream stream_;
  uint64_t written_ = 0;
};

/**
 * @brief File-backed source (infrastructure adapter).
 * Throws IoError on open failures.
 */
class FileByteSource : public IByteSource {
public:
  explicit FileByteSource(const std::filesystem::path &path);
  ~FileByteSource() override = default;

  uint64_t size() const override { return size_; }
  uint64_t position() override;
  void seek(uint64_t position) override;
  std::size_t read(uint8_t *dst, std::size_t count) override;

private:
  std::ifstream stream_;
  uint64_t size_ = 0;
};

} // namespace core
} // namespace compression
