#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace compression {
namespace core {

/**
 * @brief Non-owning view over contiguous bytes (C++17 std::span shim).
 *
 * The referenced buffer must outlive the view. Read-only by design;
 * producers write through IByteSink instead.
 */
class ByteView {
public:
  using value_type = uint8_t;
  using const_iterator = const uint8_t *;

  static constexpr std::size_t npos = static_cast<std::size_t>(-1);

  ByteView() = default;

  ByteView(const uint8_t *data, std::size_t size) : data_(data), size_(size) {}

  ByteView(const void *data, std::size_t size)
      : data_(static_cast<const uint8_t *>(data)), size_(size) {}

  ByteView(const std::vector<uint8_t> &data)
      : data_(data.data()), size_(data.size()) {}

  ByteView(const std::string &data)
      : data_(reinterpret_cast<const uint8_t *>(data.data())),
        size_(data.size()) {}

  template <std::size_t N>
  ByteView(const std::array<uint8_t, N> &data)
      : data_(data.data()), size_(N) {}

  const uint8_t *data() const { return data_; }
  std::size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  const_iterator begin() const { return data_; }
  const_iterator end() const { return data_ + size_; }

  uint8_t operator[](std::size_t index) const { return data_[index]; }

  uint8_t at(std::size_t index) const {
    if (index >= size_) {
      throw std::out_of_range("ByteView::at: index out of range");
    }
    return data_[index];
  }

  /**
   * @brief Returns a sub-view. Throws std::out_of_range on invalid bounds.
   */
  ByteView subspan(std::size_t offset, std::size_t count = npos) const {
    if (offset > size_) {
      throw std::out_of_range("ByteView::subspan: offset out of range");
    }
    const std::size_t remaining = size_ - offset;
    const std::size_t n = (count == npos) ? remaining : count;
    if (n > remaining) {
      throw std::out_of_range("ByteView::subspan: count out of range");
    }
    return ByteView(data_ + offset, n);
  }

private:
  const uint8_t *data_ = nullptr;
  std::size_t size_ = 0;
};

} // namespace core
} // namespace compression
