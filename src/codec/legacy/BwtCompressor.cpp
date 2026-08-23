#include <algorithm>
#include <compression/codec/legacy/ArithmeticCompressor.hpp>
#include <compression/codec/legacy/BwtCompressor.hpp>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace compression {

uint32_t read_u32_be(const std::vector<uint8_t> &src, size_t &pos) {
  if (pos + 4 > src.size()) {
    throw std::out_of_range("read_u32_be: insufficient bytes");
  }
  uint32_t b0 = static_cast<uint32_t>(src[pos++]);
  uint32_t b1 = static_cast<uint32_t>(src[pos++]);
  uint32_t b2 = static_cast<uint32_t>(src[pos++]);
  uint32_t b3 = static_cast<uint32_t>(src[pos++]);
  return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

static void write_u32_be(std::vector<uint8_t> &dest, uint32_t value) {
  dest.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  dest.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  dest.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  dest.push_back(static_cast<uint8_t>(value & 0xFF));
}

// Escape data so 0x00 can be used as unique EOF marker.
// 0x00 -> 0xFF 0x01 0x00,  0xFF -> 0xFF 0xFF
static std::vector<uint8_t> escape_for_bwt(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> out;
  out.reserve(data.size() * 3);
  for (uint8_t b : data) {
    if (b == 0x00) {
      out.push_back(0xFF); out.push_back(0x01); out.push_back(0x00);
    } else if (b == 0xFF) {
      out.push_back(0xFF); out.push_back(0xFF);
    } else {
      out.push_back(b);
    }
  }
  return out;
}

static std::vector<uint8_t> unescape_bwt(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> out;
  out.reserve(data.size());
  for (size_t i = 0; i < data.size(); ) {
    if (data[i] == 0xFF) {
      if (i + 1 >= data.size()) throw std::runtime_error("BWT unescape: truncated");
      if (data[i + 1] == 0x01) {
        if (i + 2 >= data.size()) throw std::runtime_error("BWT unescape: truncated");
        out.push_back(0x00); i += 3;
      } else if (data[i + 1] == 0xFF) {
        out.push_back(0xFF); i += 2;
      } else {
        throw std::runtime_error("BWT unescape: invalid escape");
      }
    } else {
      out.push_back(data[i]); i++;
    }
  }
  return out;
}

std::vector<uint8_t>
BwtCompressor::compress(const std::vector<uint8_t> &data) const {
  if (data.empty()) return {};

  // Escape + EOF sentinel for correct primary index
  auto escaped = escape_for_bwt(data);
  escaped.push_back(0x00);  // EOF marker — lexicographically smallest, unique

  auto bwt_transformed = bwt_transform(escaped);
  auto mtf_encoded = mtf_encode(bwt_transformed);

  ArithmeticCompressor arithmetic;
  auto arith_compressed = arithmetic.compress(mtf_encoded);

  // Header: magic(1) + version(1) + original_size(4) + compressed
  std::vector<uint8_t> result;
  result.push_back(0xB7);  // magic
  result.push_back(0x01);  // version 1 = with EOF sentinel
  write_u32_be(result, static_cast<uint32_t>(data.size()));
  result.insert(result.end(), arith_compressed.begin(), arith_compressed.end());
  return result;
}

std::vector<uint8_t>
BwtCompressor::decompress(const std::vector<uint8_t> &data) const {
  if (data.empty()) return {};
  if (data.size() < 6)
    throw std::runtime_error("BWT decompress: header too short");

  // Version 1: EOF sentinel format
  if (data[0] == 0xB7 && data[1] == 0x01) {
    size_t pos = 2;
    uint32_t original_size = read_u32_be(data, pos);
    if (original_size == 0) return {};

    std::vector<uint8_t> arith_data(data.begin() + pos, data.end());
    ArithmeticCompressor arithmetic;
    auto mtf_encoded = arithmetic.decompress(arith_data);
    auto bwt_data = mtf_decode(mtf_encoded);
    auto with_eof = inverse_bwt_transform(bwt_data);

    // Strip EOF marker
    if (with_eof.empty() || with_eof.back() != 0x00)
      throw std::runtime_error("BWT decompress: EOF marker missing");
    with_eof.pop_back();

    return unescape_bwt(with_eof);
  }

  // Legacy: no header, plain arithmetic-coded MTF
  ArithmeticCompressor arithmetic;
  auto mtf_encoded = arithmetic.decompress(data);
  auto mtf_decoded = mtf_decode(mtf_encoded);
  return inverse_bwt_transform(mtf_decoded);
}

std::vector<uint8_t>
BwtCompressor::transform(const std::vector<uint8_t> &data) const {
  if (data.empty()) {
    return {};
  }

  return bwt_transform(data);
}

std::vector<uint8_t>
BwtCompressor::inverseTransform(const std::vector<uint8_t> &data) const {
  if (data.empty()) {
    return {};
  }

  return inverse_bwt_transform(data);
}

// --- Helper Implementations ---

std::vector<uint8_t>
BwtCompressor::bwt_transform(const std::vector<uint8_t> &data) const {
  const size_t n = data.size();
  if (n == 0)
    return {};

  // Optimized Cyclic Doubling Algorithm
  // We work directly with cyclic shifts of length N, avoiding the 2*N
  // allocation.

  std::vector<int32_t> sa(n);
  std::vector<int32_t> rank(n);
  std::vector<int32_t> new_rank(n);

  // Initial ranks based on first character
  for (size_t i = 0; i < n; ++i) {
    sa[i] = static_cast<int32_t>(i);
    rank[i] = data[i];
  }

  // Doubling steps
  for (size_t k = 1; k < n; k <<= 1) {
    auto compare = [&](int32_t i, int32_t j) {
      if (rank[i] != rank[j]) {
        return rank[i] < rank[j];
      }
      // Handle cyclic shift
      int32_t ri = (i + k < n) ? rank[i + k] : rank[i + k - n];
      int32_t rj = (j + k < n) ? rank[j + k] : rank[j + k - n];
      return ri < rj;
    };

    std::sort(sa.begin(), sa.end(), compare);

    // Re-rank
    new_rank[sa[0]] = 0;
    for (size_t i = 1; i < n; ++i) {
      int32_t s1 = sa[i];
      int32_t s0 = sa[i - 1];

      bool equivalent = false;
      if (rank[s1] == rank[s0]) {
        int32_t r1 = (s1 + k < n) ? rank[s1 + k] : rank[s1 + k - n];
        int32_t r0 = (s0 + k < n) ? rank[s0 + k] : rank[s0 + k - n];
        equivalent = (r1 == r0);
      }

      new_rank[s1] = new_rank[s0] + (equivalent ? 0 : 1);
    }

    rank = new_rank;

    // Optimization: if all ranks are distinct, we are done
    if (rank[sa[n - 1]] == static_cast<int32_t>(n - 1))
      break;
  }

  // Construct output
  // The BWT is the last column of the sorted cyclic rotations matrix.
  // L[i] = S[(sa[i] + n - 1) % n]

  std::vector<uint8_t> out;
  out.reserve(4 + n);

  // Find primary index (where the original string is in the sorted list)
  // The original string corresponds to rotation starting at 0.
  // So we look for i such that sa[i] == 0.

  uint32_t primary_index = 0;

  // We can construct the output and find primary index in one pass
  // Placeholder for primary index
  out.resize(4);

  for (size_t i = 0; i < n; ++i) {
    int32_t start = sa[i];
    if (start == 0) {
      primary_index = static_cast<uint32_t>(i);
      out.push_back(data[n - 1]); // (0 + n - 1) % n = n - 1
    } else {
      out.push_back(data[start - 1]);
    }
  }

  // Write primary index
  out[0] = static_cast<uint8_t>((primary_index >> 24) & 0xFF);
  out[1] = static_cast<uint8_t>((primary_index >> 16) & 0xFF);
  out[2] = static_cast<uint8_t>((primary_index >> 8) & 0xFF);
  out[3] = static_cast<uint8_t>(primary_index & 0xFF);

  return out;
}

std::vector<uint8_t>
BwtCompressor::inverse_bwt_transform(const std::vector<uint8_t> &data) const {
  if (data.empty()) {
    return {};
  }
  if (data.size() < 4) {
    throw std::runtime_error(
        "Invalid BWT data: too short to contain primary index.");
  }

  size_t pos = 0;
  uint32_t primary_index = read_u32_be(data, pos);

  const uint8_t *data_ptr = data.data() + 4;
  const size_t n = data.size() - 4;

  if (n == 0) {
    return {};
  }

  // Use efficient LF mapping inverse for ALL sizes - the iterative approach is
  // too slow
  if (primary_index >= n) {
    throw std::runtime_error(
        "Invalid BWT data: primary index is out of bounds.");
  }

  std::vector<uint8_t> decompressed_data(n);

  // Correct LF mapping algorithm for BWT inverse
  // Build the first column (sorted last column)
  std::vector<uint8_t> first_column(data_ptr, data_ptr + n);
  std::sort(first_column.begin(), first_column.end());

  // Count occurrences of each character in first column
  std::vector<uint32_t> char_counts(256, 0);
  for (uint8_t c : first_column) {
    char_counts[c]++;
  }

  // Build character position table for first column
  std::vector<uint32_t> char_start(256);
  uint32_t start_pos = 0;
  for (int c = 0; c < 256; ++c) {
    char_start[c] = start_pos;
    start_pos += char_counts[c];
  }

  // Build LF mapping: for each position in last column, find corresponding
  // position in first column
  std::vector<uint32_t> lf_mapping(n);
  std::vector<uint32_t> char_pos(256, 0);

  for (size_t i = 0; i < n; ++i) {
    uint8_t c = data_ptr[i];
    lf_mapping[i] = char_start[c] + char_pos[c];
    char_pos[c]++;
  }

  // Reconstruct original string by following LF chain using the last column (L)
  // Emit L[row] then step to next row
  uint32_t current = primary_index;
  for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
    decompressed_data[static_cast<size_t>(i)] = data_ptr[current];
    current = lf_mapping[current];
  }

  return decompressed_data;
}

std::vector<uint8_t>
BwtCompressor::mtf_encode(const std::vector<uint8_t> &data) {
  if (data.empty()) {
    return {};
  }

  std::vector<uint8_t> symbol_table(256);
  std::iota(symbol_table.begin(), symbol_table.end(), 0);
  std::vector<uint8_t> result;
  result.reserve(data.size());

  for (uint8_t symbol : data) {
    auto it = std::find(symbol_table.begin(), symbol_table.end(), symbol);
    size_t rank = std::distance(symbol_table.begin(), it);
    result.push_back(static_cast<uint8_t>(rank));

    // Move symbol to front
    std::rotate(symbol_table.begin(), it, it + 1);
  }
  return result;
}

std::vector<uint8_t>
BwtCompressor::mtf_decode(const std::vector<uint8_t> &data) {
  if (data.empty()) {
    return {};
  }

  std::vector<uint8_t> symbol_table(256);
  std::iota(symbol_table.begin(), symbol_table.end(), 0);
  std::vector<uint8_t> result;
  result.reserve(data.size());

  for (uint8_t rank : data) {
    uint8_t symbol = symbol_table[rank];
    result.push_back(symbol);

    // Move symbol to front
    std::rotate(symbol_table.begin(), symbol_table.begin() + rank, symbol_table.begin() + rank + 1);
  }
  return result;
}

} // namespace compression
