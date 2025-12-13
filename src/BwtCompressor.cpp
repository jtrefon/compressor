#include <algorithm>
#include <compression/ArithmeticCompressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace compression {

// Helper to write a 32-bit integer to a byte vector in big-endian format.
void write_u32_be(std::vector<uint8_t> &dest, uint32_t value) {
  dest.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  dest.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  dest.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  dest.push_back(static_cast<uint8_t>(value & 0xFF));
}

// Helper to read a 32-bit integer from a byte vector in big-endian format.
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

std::vector<uint8_t>
BwtCompressor::compress(const std::vector<uint8_t> &data) const {
  if (data.empty()) {
    return {};
  }

  const auto bwt_transformed = bwt_transform(data);
  const auto mtf_encoded = mtf_encode(bwt_transformed);

  // Directly use Arithmetic Coding on MTF output
  // MTF produces skewed data (many small numbers), ideal for adaptive
  // arithmetic coding.
  ArithmeticCompressor arithmetic;
  return arithmetic.compress(mtf_encoded);
}

std::vector<uint8_t>
BwtCompressor::decompress(const std::vector<uint8_t> &data) const {
  if (data.empty()) {
    return {};
  }

  // Decompress Arithmetic first
  ArithmeticCompressor arithmetic;
  const auto mtf_encoded = arithmetic.decompress(data);

  const auto mtf_decoded = mtf_decode(mtf_encoded);

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
    symbol_table.erase(it);
    symbol_table.insert(symbol_table.begin(), symbol);
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
    symbol_table.erase(symbol_table.begin() + rank);
    symbol_table.insert(symbol_table.begin(), symbol);
  }
  return result;
}

} // namespace compression
