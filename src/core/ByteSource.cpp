#include <compression/core/ByteSource.hpp>

#include <algorithm>
#include <cstring>

namespace compression {
namespace core {

void MemoryByteSink::write(ByteView data) {
  data_.insert(data_.end(), data.begin(), data.end());
}

void MemoryByteSource::seek(uint64_t position) {
  if (position > data_.size()) {
    throw std::out_of_range("MemoryByteSource::seek: position beyond end");
  }
  position_ = position;
}

std::size_t MemoryByteSource::read(uint8_t *dst, std::size_t count) {
  const uint64_t remaining = data_.size() - position_;
  const std::size_t n =
      static_cast<std::size_t>(std::min<uint64_t>(remaining, count));
  if (n > 0) {
    std::memcpy(dst, data_.data() + position_, n);
    position_ += n;
  }
  return n;
}

FileByteSink::FileByteSink(const std::filesystem::path &path)
    : stream_(path, std::ios::binary | std::ios::trunc) {
  if (!stream_.is_open()) {
    throw IoError("Cannot open file for writing: " + path.string());
  }
}

void FileByteSink::write(ByteView data) {
  stream_.write(reinterpret_cast<const char *>(data.data()),
                static_cast<std::streamsize>(data.size()));
  if (!stream_) {
    throw IoError("Error writing to file");
  }
  written_ += data.size();
}

FileByteSource::FileByteSource(const std::filesystem::path &path)
    : stream_(path, std::ios::binary) {
  if (!stream_.is_open()) {
    throw IoError("Cannot open file for reading: " + path.string());
  }
  stream_.seekg(0, std::ios::end);
  const std::streamoff end = stream_.tellg();
  stream_.seekg(0, std::ios::beg);
  size_ = end > 0 ? static_cast<uint64_t>(end) : 0;
}

uint64_t FileByteSource::position() {
  const std::streamoff pos = stream_.tellg();
  return pos > 0 ? static_cast<uint64_t>(pos) : 0;
}

void FileByteSource::seek(uint64_t position) {
  stream_.seekg(static_cast<std::streamoff>(position), std::ios::beg);
  if (!stream_) {
    throw std::out_of_range("FileByteSource::seek: position beyond end");
  }
}

std::size_t FileByteSource::read(uint8_t *dst, std::size_t count) {
  stream_.read(reinterpret_cast<char *>(dst),
               static_cast<std::streamsize>(count));
  const std::size_t got = static_cast<std::size_t>(stream_.gcount());
  return got;
}

} // namespace core
} // namespace compression
