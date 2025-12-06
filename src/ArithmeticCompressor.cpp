#include <algorithm>
#include <cmath>
#include <compression/ArithmeticCompressor.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace compression {

// Basic CRC32 implementation (Poly: 0xEDB88320)
static uint32_t calculate_crc32(const std::vector<uint8_t> &data) {
  uint32_t crc = 0xFFFFFFFF;
  for (uint8_t byte : data) {
    crc ^= byte;
    for (int k = 0; k < 8; ++k) {
      crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
  }
  return ~crc;
}

// --- ArithmeticCompressor Implementation ---

std::vector<uint8_t>
ArithmeticCompressor::compress(const std::vector<uint8_t> &data) const {
  if (data.empty())
    return {};

  // 1. Header: Original Size (4 bytes) + CRC32 (4 bytes)
  std::vector<uint8_t> result;
  uint32_t originalSize = static_cast<uint32_t>(data.size());
  for (int i = 0; i < 4; ++i)
    result.push_back((originalSize >> (i * 8)) & 0xFF);

  uint32_t checksum = calculate_crc32(data);
  for (int i = 0; i < 4; ++i)
    result.push_back((checksum >> (i * 8)) & 0xFF);

  // 2. Encode
  AdaptiveContextModel model;
  RangeEncoder encoder;
  encoder.start(result);
  uint8_t context = 0;

  for (uint8_t symbol : data) {
    uint32_t low, high, total;
    model.getProbability(context, symbol, low, high, total);
    encoder.encode(low, high, total, result);
    model.update(context, symbol);
    context = symbol;
  }
  encoder.finish(result);
  return result;
}

std::vector<uint8_t>
ArithmeticCompressor::decompress(const std::vector<uint8_t> &data) const {
  if (data.empty())
    return {};
  if (data.size() < 8)
    throw std::runtime_error(
        "Truncated compressed data header"); // Need 8 bytes now
  size_t offset = 0;
  uint32_t originalSize = 0;
  for (int i = 0; i < 4; ++i)
    originalSize |= (static_cast<uint32_t>(data[offset++]) << (i * 8));

  uint32_t expectedChecksum = 0;
  for (int i = 0; i < 4; ++i)
    expectedChecksum |= (static_cast<uint32_t>(data[offset++]) << (i * 8));

  if (originalSize == 0)
    return {};

  AdaptiveContextModel model;
  RangeDecoder decoder;
  std::vector<uint8_t> result;
  result.reserve(originalSize);
  decoder.start(data, offset);
  uint8_t context = 0;

  for (size_t i = 0; i < originalSize; ++i) {
    // We need total from the model for the current context. Using
    // getProbability(..., 0) to fetch total.
    uint32_t dummy_l, dummy_h, total;
    model.getProbability(context, 0, dummy_l, dummy_h, total);

    uint32_t count = decoder.getCurrentCount(total);

    uint32_t low, high, check_total;
    uint8_t symbol = model.getSymbol(context, count, low, high, check_total);

    result.push_back(symbol);
    decoder.removeRange(low, high, total, data, offset);
    model.update(context, symbol);
    context = symbol;
  }

  // Verify CRC32
  if (calculate_crc32(result) != expectedChecksum) {
    throw std::runtime_error("CRC32 Checksum mismatch: Data corrupted");
  }

  return result;
}

// --- Adaptive Context Model ---

ArithmeticCompressor::AdaptiveContextModel::AdaptiveContextModel()
    : contexts_(256) {
  // Initialize all contexts with uniform probability (count 1 for each symbol)
  for (auto &ctx : contexts_) {
    // Fenwick tree 1-based indexing. Size 257.
    // Increment each position by 1.
    // Naive init O(256 * 256) is fine once.
    for (int i = 0; i < 256; ++i) {
      increment(ctx, i + 1, 1);
    }
  }
}

void ArithmeticCompressor::AdaptiveContextModel::increment(Context &ctx,
                                                           int index, int val) {
  while (index < ctx.tree.size()) {
    ctx.tree[index] += val;
    index += index & (-index);
  }
  ctx.total += val;
}

uint32_t ArithmeticCompressor::AdaptiveContextModel::query(const Context &ctx,
                                                           int index) const {
  uint32_t sum = 0;
  while (index > 0) {
    sum += ctx.tree[index];
    index -= index & (-index);
  }
  return sum;
}

void ArithmeticCompressor::AdaptiveContextModel::rescale(Context &ctx) {
  // Halve all counts to prevent overflow
  // Since we can't easily iterate values in Fenwick, we must reconstruct.
  // However, for compression, simple linear scan on `tree` doesn't give
  // individual values. WE NEED TO STORE COUNTS SEPARATELY if we want to rescale
  // exactly. optimization: Just use MAX_TOTAL huge enough to avoid frequent
  // rescale? Or reconstruct: val[i] = query(i) - query(i-1).

  std::vector<uint32_t> values(256);
  for (int i = 0; i < 256; ++i) {
    uint32_t count = query(ctx, i + 1) - query(ctx, i);
    values[i] = (count + 1) / 2; // Halve, round up to avoid 0
    if (values[i] == 0)
      values[i] = 1; // Safety
  }

  // Rebuild tree
  std::fill(ctx.tree.begin(), ctx.tree.end(), 0);
  ctx.total = 0;
  for (int i = 0; i < 256; ++i) {
    increment(ctx, i + 1, values[i]);
  }
}

void ArithmeticCompressor::AdaptiveContextModel::getProbability(
    uint8_t context, uint8_t symbol, uint32_t &low, uint32_t &high,
    uint32_t &total) const {
  const auto &ctx = contexts_[context];
  low = query(ctx, symbol);      // Sum(0..symbol-1)
  high = query(ctx, symbol + 1); // Sum(0..symbol)
  total = ctx.total;
}

void ArithmeticCompressor::AdaptiveContextModel::update(uint8_t context,
                                                        uint8_t symbol) {
  auto &ctx = contexts_[context];
  // Increase update weight to speed up adaptation
  // Initial total is 256 (1 per symbol).
  // Using 16 allows learning distribution in ~16 steps instead of 256.
  increment(ctx, symbol + 1, 16);
  if (ctx.total > MAX_TOTAL) {
    rescale(ctx);
  }
}

uint8_t ArithmeticCompressor::AdaptiveContextModel::getSymbol(
    uint8_t context, uint32_t count, uint32_t &low, uint32_t &high,
    uint32_t &total) const {
  const auto &ctx = contexts_[context];
  total = ctx.total;

  // Binary search over prefix sums for O(log N) would be ideal,
  // but linear scan over 256 items is ~100-200 ops, totally fine for now
  // compared to Range Coder arithmetic.
  uint32_t current_low = 0;
  for (int i = 0; i < 256; ++i) {
    // query(ctx, i+1) gives cumulative up to i
    // Optimization: track cumulative sum linearly
    // Wait, 'query' is O(log N). 256 * log N is slow.
    // We really just need to traverse.
    // Since we treat Fenwick indices 1..256, but we can't iterate efficiently
    // without rebuilding. Let's use `query` but binary search? Or just optimize
    // later. Let's rely on `query` for correctness first, even if O(256 * 8).
    // It's 2k ops. Might be bottleneck.

    // BETTER: maintain a cached 'tree' or just use the Fenwick structure to
    // binary lift. Implementing Binary Lifting on Fenwick Tree for O(log N): We
    // search for index 'idx' such that query(idx-1) <= count < query(idx).

    // Standard Binary Lifting on BIT
    /*
    int idx = 0;
    int sum = 0;
    for (int bit_mask = 128; bit_mask > 0; bit_mask >>= 1) {
        int tIdx = idx + bit_mask;
        if (tIdx < 257 && sum + ctx.tree[tIdx] <= count) {
            idx = tIdx;
            sum += ctx.tree[idx];
        }
    }
    return idx; // idx is 0-255? No, BIT is 1-based.
    */

    // Let's stick to safe Linear Scan for correctness first, straightforward
    // debug.
    uint32_t sym_freq = query(ctx, i + 1) - query(ctx, i);
    if (current_low + sym_freq > count) {
      low = current_low;
      high = current_low + sym_freq;
      return static_cast<uint8_t>(i);
    }
    current_low += sym_freq;
  }
  // Should not reach here if count < total
  return 0;
}

// --- Range Encoder ---

void ArithmeticCompressor::RangeEncoder::start(std::vector<uint8_t> &output) {
  low_ = 0;
  range_ = MAX_RANGE;
  help_ = 0;
  buffer_ = 0;
  buffer_count_ = 0;
  buffer_full_ = false;
}

void ArithmeticCompressor::RangeEncoder::encode(uint32_t low_count,
                                                uint32_t high_count,
                                                uint32_t total,
                                                std::vector<uint8_t> &output) {
  uint32_t r = range_ / total;
  uint32_t size_part = r * (high_count - low_count);
  uint64_t new_low = low_ + static_cast<uint64_t>(r) * low_count;

  // Check for carry
  if (new_low >= (1ULL << 32)) {
    buffer_++;
    // Force output of carry
    output.push_back(buffer_);
    for (int i = 0; i < buffer_count_; ++i)
      output.push_back(0);

    buffer_ = 0; // Reset buffer (though it's "empty" now)
    buffer_count_ = 0;
    buffer_full_ = false; // Buffer is empty/consumed
    new_low &= 0xFFFFFFFF;
  }
  low_ = new_low;
  range_ = size_part;

  while (range_ < TOP_VALUE) {
    if (low_ < (0xFF000000)) {
      uint8_t b = (low_ >> 24) & 0xFF;
      // Write previous buffer and FFs if buffer is full
      if (buffer_full_) {
        output.push_back(buffer_);
        for (int i = 0; i < buffer_count_; ++i)
          output.push_back(0xFF);
        buffer_count_ = 0;
      }
      buffer_ = b;
      buffer_full_ = true;
    } else if (low_ >= 0xFF000000) {
      buffer_count_++;
      low_ &= 0x00FFFFFF;
    }

    low_ = (low_ << 8) & 0xFFFFFFFF;
    range_ <<= 8;
  }
}

void ArithmeticCompressor::RangeEncoder::outputByte(
    uint8_t b, std::vector<uint8_t> &output) {
  // Helper handled inline
}

void ArithmeticCompressor::RangeEncoder::finish(std::vector<uint8_t> &output) {
  for (int i = 0; i < 5; ++i) {
    if (low_ < (0xFF000000)) {
      uint8_t b = (low_ >> 24) & 0xFF;
      if (buffer_full_) {
        output.push_back(buffer_);
        for (int c = 0; c < buffer_count_; ++c)
          output.push_back(0xFF);
        buffer_count_ = 0;
      }
      buffer_ = b;
      buffer_full_ = true;
    } else {
      buffer_count_++;
      low_ &= 0x00FFFFFF;
    }
    low_ = (low_ << 8) & 0xFFFFFFFF;
  }
}

// --- Range Decoder ---

void ArithmeticCompressor::RangeDecoder::start(
    const std::vector<uint8_t> &input, size_t &inputPos) {
  low_ = 0;
  range_ = MAX_RANGE;
  code_ = 0;

  // Fill buffer with first 4 bytes (32 bits)
  for (int i = 0; i < 4; ++i) {
    code_ <<= 8;
    if (inputPos < input.size()) {
      code_ |= input[inputPos++];
    }
  }
}

uint32_t
ArithmeticCompressor::RangeDecoder::getCurrentCount(uint32_t total) const {
  uint32_t r = range_ / total;
  return (code_ - static_cast<uint32_t>(low_)) / r;
}

void ArithmeticCompressor::RangeDecoder::removeRange(
    uint32_t low_count, uint32_t high_count, uint32_t total,
    const std::vector<uint8_t> &input, size_t &inputPos) {
  uint32_t r = range_ / total;
  uint32_t size_part = r * (high_count - low_count);
  uint32_t start_low_part = r * low_count;

  // My Decoder struct has `low_`.
  // Let's ignore `low_` in decoder if we just shift `code`.
  // But `getCurrentCount` used `(code_ - low_)`.
  // If I use `low_` to track the base, then:

  low_ = (low_ + start_low_part) & 0xFFFFFFFF; // Manually wrap at 32-bit
  range_ = size_part;

  while (range_ < TOP_VALUE) {
    // Just always shift if range < TOP_VALUE, matching Encoder logic
    uint8_t byte = 0;
    if (inputPos < input.size())
      byte = input[inputPos++];

    code_ = (code_ << 8) | byte;
    low_ = (low_ << 8) & 0xFFFFFFFF;
    range_ <<= 8;
  }
}

} // namespace compression
