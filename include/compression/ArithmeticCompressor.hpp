#pragma once

#include "ICompressor.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace compression {

/**
 * @class ArithmeticCompressor
 * @brief Implements arithmetic coding for near-optimal entropy compression
 *
 * Arithmetic coding can achieve compression ratios closer to the theoretical
 * entropy limit compared to Huffman coding, typically providing 5-15% better
 * compression on the same data.
 *
 * This implementation uses adaptive probability modeling for better performance
 * on diverse data types.
 */
class ArithmeticCompressor : public ICompressor {
public:
  // Type aliases
  using FrequencyMap = std::map<uint8_t, uint64_t>;
  using ProbabilityMap = std::map<uint8_t, double>;

  std::vector<uint8_t>
  compress(const std::vector<uint8_t> &data) const override;
  std::vector<uint8_t>
  decompress(const std::vector<uint8_t> &data) const override;

private:
  // Constants for arithmetic coding precision (32-bit Ref: Michael Schindler /
  // compression.ru) We use a 32-bit range, but calculations often require
  // 64-bit to avoid overflow.
  static constexpr uint32_t CODE_BITS = 32;
  static constexpr uint32_t TOP_VALUE = 1u
                                        << 24; // Threshold for renormalization
  static constexpr uint32_t MAX_RANGE = 0xFFFFFFFF; // Full 32-bit range

  // Adaptive Order-1 Context Model
  class AdaptiveContextModel {
  public:
    AdaptiveContextModel();
    // Get cumulative frequency range for a symbol in a given context
    void getProbability(uint8_t context, uint8_t symbol, uint32_t &low,
                        uint32_t &high, uint32_t &total) const;

    // Find symbol for a given cumulative count [0..total-1]
    // Returns symbol and sets its low/high/total bounds
    uint8_t getSymbol(uint8_t context, uint32_t count, uint32_t &low,
                      uint32_t &high, uint32_t &total) const;

    // Update frequency of a symbol in a given context
    void update(uint8_t context, uint8_t symbol);

  private:
    static constexpr int SYMBOLS =
        257; // 0-255 + EOF? Or just 256 if we track size explicitly.
             // We track size explicitly, so 256 is enough.
    static constexpr int MAX_TOTAL =
        65535; // Rescale threshold (higher precision for better compression)

    struct Context {
      std::vector<uint32_t> tree; // Fenwick Tree
      uint32_t total;

      Context() : tree(257, 0), total(0) {}
    };

    std::vector<Context> contexts_; // 256 contexts (one for each prev byte)

    void increment(Context &ctx, int index, int val);
    uint32_t query(const Context &ctx, int index) const;
    void rescale(Context &ctx);
  };

  AdaptiveContextModel model_;
  class RangeEncoder {
  public:
    void start(std::vector<uint8_t> &output);
    void encode(uint32_t low_count, uint32_t high_count, uint32_t total,
                std::vector<uint8_t> &output);
    void finish(std::vector<uint8_t> &output);

  private:
    uint64_t low_ = 0;
    uint32_t range_ = 0xFFFFFFFF;
    uint32_t help_ = 0;    // Bytes to follow
    uint8_t buffer_ = 0;   // Current byte
    int buffer_count_ = 0; // Counter for 'help' bytes
    bool buffer_full_ = false;

    // Output a byte with carry handling
    void outputByte(uint8_t b, std::vector<uint8_t> &output);
  };

  class RangeDecoder {
  public:
    void start(const std::vector<uint8_t> &input, size_t &inputPos);
    uint32_t getCurrentCount(uint32_t total) const;
    void removeRange(uint32_t low_count, uint32_t high_count, uint32_t total,
                     const std::vector<uint8_t> &input, size_t &inputPos);

  private:
    uint64_t low_ = 0;
    uint32_t range_ = 0xFFFFFFFF;
    uint32_t code_ = 0;
  };
};

} // namespace compression
