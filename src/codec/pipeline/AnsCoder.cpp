#include <compression/codec/pipeline/AnsCoder.hpp>

#include <compression/core/Errors.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

namespace compression {
namespace codec {
namespace pipeline {

namespace {

// Frequency table scale (1 << kScaleBits is the slot modulus / table sum).
constexpr uint32_t kScaleBits = 16;
constexpr uint32_t kFreqSum = 1u << kScaleBits; // 65536
constexpr uint32_t kSymbolCount = 256;
// The byte-aligned coder needs freq <= L / 256: the encoder renormalizes to
// x < ((L >> scale_bits) << 8) * freq = 2^15 * freq, and the step then stays
// below 2^31 + 2^17 in 32 bits. With L = 2^23 that caps each frequency at
// 2^15 (a symbol above 50% of the input must not exceed it, and a u16 can
// never hold kFreqSum anyway).
constexpr uint32_t kMaxFreq = kFreqSum / 2; // 32768
// Renormalization threshold (the ryg_rans byte-aligned constants).
constexpr uint32_t kStateLow = 1u << 23;
// Absolute cap on the symbol count: bounds decode work and output size for
// hostile headers (each symbol emits >= 0 renorm bytes, so the payload size
// alone cannot bound it).
constexpr uint32_t kMaxSymbols = 1u << 24;
// freqs(512) + count(4) + state(4)
constexpr size_t kHeaderSize = kSymbolCount * 2 + 4 + 4;

struct Table {
  std::array<uint16_t, kSymbolCount> freqs{};
  std::array<uint32_t, kSymbolCount + 1> cdf{};
};

// Every symbol gets a frequency in [1, kMaxFreq], the table sums to exactly
// kFreqSum, and the resulting state math stays within 32 bits (see kMaxFreq).
Table buildTable(const std::array<uint32_t, kSymbolCount> &counts) {
  Table table;
  uint64_t total = 0;
  for (const uint32_t c : counts) {
    total += c;
  }
  if (total == 0) {
    // Uniform empty distribution: exactly kFreqSum / 256 per symbol.
    const uint16_t uniform = static_cast<uint16_t>(kFreqSum / kSymbolCount);
    for (uint32_t s = 0; s < kSymbolCount; ++s) {
      table.freqs[s] = uniform;
    }
  } else {
    for (uint32_t s = 0; s < kSymbolCount; ++s) {
      uint64_t freq = counts[s] * kFreqSum / total;
      freq = std::max<uint64_t>(freq, 1);
      freq = std::min<uint64_t>(freq, kMaxFreq);
      table.freqs[s] = static_cast<uint16_t>(freq);
    }
    uint32_t sum = 0;
    for (const uint16_t f : table.freqs) {
      sum += f;
    }
    while (sum > kFreqSum) {
      auto it = std::max_element(table.freqs.begin(), table.freqs.end());
      if (*it == 1) {
        throw core::ConfigurationError("ANS table normalization failed");
      }
      --(*it);
      --sum;
    }
    // At most one symbol can exceed 50% of the input, so at most one entry is
    // pinned at kMaxFreq and the deficit always fits into an entry below the
    // cap (every entry is >= 1).
    while (sum < kFreqSum) {
      uint32_t best = 0;
      for (uint32_t s = 1; s < kSymbolCount; ++s) {
        if (table.freqs[s] < kMaxFreq &&
            (table.freqs[best] >= kMaxFreq ||
             table.freqs[s] > table.freqs[best])) {
          best = s;
        }
      }
      const uint32_t room =
          table.freqs[best] < kMaxFreq ? kMaxFreq - table.freqs[best] : 0;
      if (room == 0) {
        throw core::ConfigurationError("ANS table normalization failed");
      }
      const uint32_t add = std::min(room, kFreqSum - sum);
      table.freqs[best] = static_cast<uint16_t>(table.freqs[best] + add);
      sum += add;
    }
  }
  uint32_t prefix = 0;
  for (uint32_t s = 0; s < kSymbolCount; ++s) {
    table.cdf[s] = prefix;
    prefix += table.freqs[s];
  }
  table.cdf[kSymbolCount] = prefix;
  return table;
}

void writeU32(std::vector<uint8_t> &out, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    out.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
  }
}

uint32_t readU32(core::ByteView data, size_t pos) {
  uint32_t value = 0;
  for (int i = 0; i < 4; ++i) {
    value |= static_cast<uint32_t>(data[pos + i]) << (8 * i);
  }
  return value;
}

} // namespace

// Static rANS (byte-aligned), the proven ryg_rans scheme:
//   * Encoder processes symbols in REVERSE order. Before each step it
//     renormalizes down (emitting bytes) to x < ((L >> scale_bits) << 8)*freq,
//     then applies the interval step x = (x/freq) << scale_bits + x%freq + cdf.
//   * Decoder processes symbols in forward order. It reads the slot from the
//     step result, steps down, then renormalizes up (reading bytes) while
//     x < L. The renorm bytes are stored in reverse emission order so the
//     decoder reads them forward.
// The emitted payload is self-describing:
//   [256 x u16 LE frequencies (sum 2^16)]
//   [u32 LE symbol count]
//   [u32 LE final encoder state]
//   [renormalization bytes]
std::vector<uint8_t> AnsCoder::encode(core::ByteView data) const {
  std::array<uint32_t, kSymbolCount> counts{};
  for (const uint8_t b : data) {
    ++counts[b];
  }
  const Table table = buildTable(counts);

  std::vector<uint8_t> out;
  out.reserve(kHeaderSize + data.size());
  for (const uint16_t f : table.freqs) {
    out.push_back(static_cast<uint8_t>(f & 0xFF));
    out.push_back(static_cast<uint8_t>(f >> 8));
  }
  writeU32(out, static_cast<uint32_t>(data.size()));

  std::vector<uint8_t> renorm;
  renorm.reserve(data.size());
  uint32_t x = kStateLow;
  for (size_t i = data.size(); i > 0; --i) {
    const uint32_t s = data[i - 1];
    const uint32_t freq = table.freqs[s];
    const uint32_t cdf = table.cdf[s];
    const uint32_t x_max = ((kStateLow >> kScaleBits) << 8) * freq;
    if (x >= x_max) {
      do {
        renorm.push_back(static_cast<uint8_t>(x & 0xFF));
        x >>= 8;
      } while (x >= x_max);
    }
    // Safe in 32 bits: x < x_max = 2^15 * freq, so (x/freq) << 16 < 2^31 and
    // the remainder/cdf add < 2^17, keeping the result < 2^31 + 2^17.
    x = ((x / freq) << kScaleBits) + (x % freq) + cdf;
  }

  writeU32(out, x);
  std::reverse(renorm.begin(), renorm.end());
  out.insert(out.end(), renorm.begin(), renorm.end());
  return out;
}

std::vector<uint8_t> AnsCoder::decode(core::ByteView data) const {
  if (data.size() < kHeaderSize) {
    throw core::CorruptDataError("ANS payload too small");
  }
  size_t pos = 0;
  uint32_t sum = 0;
  std::array<uint32_t, kSymbolCount + 1> cdf{};
  for (uint32_t s = 0; s < kSymbolCount; ++s) {
    const uint32_t freq = data[pos] | (uint32_t(data[pos + 1]) << 8);
    pos += 2;
    cdf[s] = sum;
    sum += freq;
  }
  cdf[kSymbolCount] = sum;
  if (sum != kFreqSum) {
    throw core::CorruptDataError("ANS frequency table sum mismatch");
  }
  const uint32_t count = readU32(data, pos);
  pos += 4;
  if (count > kMaxSymbols) {
    throw core::CorruptDataError("ANS symbol count exceeds limit");
  }
  uint32_t x = readU32(data, pos);
  pos += 4;

  size_t r = pos; // renorm bytes are read forward from here
  std::vector<uint8_t> out;
  out.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t slot = x & (kFreqSum - 1);
    uint32_t s = 0;
    while (cdf[s + 1] <= slot) {
      ++s;
    }
    const uint32_t freq = cdf[s + 1] - cdf[s];
    x = freq * (x >> kScaleBits) + (slot - cdf[s]);
    if (x < kStateLow) {
      do {
        if (r >= data.size()) {
          throw core::CorruptDataError("ANS renorm stream truncated");
        }
        x = (x << 8) | data[r++];
      } while (x < kStateLow);
    }
    out.push_back(static_cast<uint8_t>(s));
  }
  if (r != data.size()) {
    throw core::CorruptDataError("ANS trailing bytes");
  }
  return out;
}

} // namespace pipeline
} // namespace codec
} // namespace compression