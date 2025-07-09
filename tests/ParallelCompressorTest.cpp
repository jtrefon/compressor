#include <gtest/gtest.h>
#include <compression/ParallelCompressor.hpp>
#include <compression/NullCompressor.hpp>

using namespace compression;

TEST(ParallelCompressorTest, RoundTrip) {
    std::vector<uint8_t> data(1024 * 10, 'A');
    auto base = std::make_unique<NullCompressor>();
    ParallelCompressor pc(std::move(base), format::AlgorithmID::NULL_COMPRESSOR, 2);
    std::vector<uint8_t> compressed = pc.compress(data);
    std::vector<uint8_t> decompressed = pc.decompress(compressed);
    EXPECT_EQ(decompressed, data);
}
