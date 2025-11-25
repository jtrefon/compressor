#include <algorithm>
#include <compression/BwtCompressor.hpp>
#include <compression/HuffmanCompressor.hpp> // Include Huffman for the final stage
#include <cstdint>
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

// --- Zero-Run-Length Encoding Helpers ---

// Encodes runs of zeros using a variable-length integer scheme.
// Non-zero values are output as value + 1 (to avoid conflict with zero run
// markers, but wait, we can just use 0 as a marker). Actually, a simpler scheme
// for byte-aligned output: If value != 0: Output value. If value == 0: Output
// 0, then output run length (varint). Note: MTF produces 0..255. If we output 0
// for a run, we need to distinguish it from a literal 0. But in MTF, 0 is the
// most common symbol. So:
// - If value != 0: Output value.
// - If value == 0: Count run length L.
//   - Output 0.
//   - Output L encoded as varint.
//   - But wait, if the decoder sees 0, it reads L.
//   - If the original data was just a single 0, L=1.
//   - So we output 0, then varint(1).
//   - This adds overhead for single zeros (2 bytes instead of 1).
//   - But MTF zeros usually come in runs.
//   - Let's try this.

static std::vector<uint8_t> zrl_encode(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> result;
  result.reserve(data.size());

  size_t i = 0;
  while (i < data.size()) {
    if (data[i] != 0) {
      result.push_back(data[i]);
      i++;
    } else {
      // Found a zero, count the run
      size_t run_length = 0;
      while (i < data.size() && data[i] == 0) {
        run_length++;
        i++;
      }

      // Output 0 marker
      result.push_back(0);

      // Encode run_length using varint (128-base)
      // We encode run_length - 1 because run_length is at least 1.
      // Actually, let's just encode run_length.
      uint64_t value = run_length;
      do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value > 0)
          byte |= 0x80;
        result.push_back(byte);
      } while (value > 0);
    }
  }
  return result;
}

static std::vector<uint8_t> zrl_decode(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> result;
  result.reserve(data.size() * 2); // Estimate

  size_t i = 0;
  while (i < data.size()) {
    if (data[i] != 0) {
      result.push_back(data[i]);
      i++;
    } else {
      // Found 0 marker, read run length
      i++; // Skip marker
      if (i >= data.size()) {
        throw std::runtime_error("ZRL decode error: truncated run length");
      }

      uint64_t run_length = 0;
      int shift = 0;
      uint8_t byte;
      do {
        if (i >= data.size()) {
          throw std::runtime_error("ZRL decode error: truncated varint");
        }
        byte = data[i++];
        run_length |= static_cast<uint64_t>(byte & 0x7F) << shift;
        shift += 7;
      } while (byte & 0x80);

      // Output zeros
      for (size_t k = 0; k < run_length; ++k) {
        result.push_back(0);
      }
    }
  }
  return result;
}

std::vector<uint8_t>
BwtCompressor::compress(const std::vector<uint8_t> &data) const {
  if (data.empty()) {
    return {};
  }

  const auto bwt_transformed = bwt_transform(data);
  const auto mtf_encoded = mtf_encode(bwt_transformed);

  // Use Zero-Run-Length Encoding instead of generic RLE
  const auto zrl_encoded = zrl_encode(mtf_encoded);

  // Final stage: Huffman coding
  HuffmanCompressor huffman;
  return huffman.compress(zrl_encoded);
}

std::vector<uint8_t>
BwtCompressor::decompress(const std::vector<uint8_t> &data) const {
  if (data.empty()) {
    return {};
  }

  // Decompress Huffman first
  HuffmanCompressor huffman;
  const auto huffman_decompressed = huffman.decompress(data);

  // Decode ZRL
  const auto zrl_decoded = zrl_decode(huffman_decompressed);

  const auto mtf_decoded = mtf_decode(zrl_decoded);

  return inverse_bwt_transform(mtf_decoded);
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
    // Sort based on pair (rank[i], rank[(i+k)%n])
    // To improve cache locality, we could pack these into a struct,
    // but for now let's rely on the smaller working set (N vs 2N).

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
