#include <compression/BwtCompressor.hpp>
#include <compression/RleCompressor.hpp> // Include RLE for chaining
#include <compression/HuffmanCompressor.hpp> // Include Huffman for the final stage
#include <vector>
#include <cstdint>
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace compression {

// Helper to write a 32-bit integer to a byte vector in big-endian format.
void write_u32_be(std::vector<uint8_t>& dest, uint32_t value) {
    dest.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    dest.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    dest.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    dest.push_back(static_cast<uint8_t>(value & 0xFF));
}

// Helper to read a 32-bit integer from a byte vector in big-endian format.
uint32_t read_u32_be(const std::vector<uint8_t>& src, size_t& pos) {
    uint32_t value = (static_cast<uint32_t>(src[pos++]) << 24) |
                     (static_cast<uint32_t>(src[pos++]) << 16) |
                     (static_cast<uint32_t>(src[pos++]) << 8)  |
                     static_cast<uint32_t>(src[pos++]);
    return value;
}

std::vector<uint8_t> BwtCompressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }

    const auto bwt_transformed = bwt_transform(data);
    const auto mtf_encoded = mtf_encode(bwt_transformed);
    
    RleCompressor rle;
    const auto rle_compressed = rle.compress(mtf_encoded);

    // Final stage: Huffman coding
    HuffmanCompressor huffman;
    return huffman.compress(rle_compressed);
}

std::vector<uint8_t> BwtCompressor::decompress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }

    // Decompress Huffman first
    HuffmanCompressor huffman;
    const auto huffman_decompressed = huffman.decompress(data);

    RleCompressor rle;
    const auto rle_decompressed = rle.decompress(huffman_decompressed);

    const auto mtf_decoded = mtf_decode(rle_decompressed);

    return inverse_bwt_transform(mtf_decoded);
}


// --- Helper Implementations ---

std::vector<uint8_t> BwtCompressor::bwt_transform(const std::vector<uint8_t>& data) const {
    const size_t n = data.size();
    std::vector<uint32_t> suffix_array(n);
    std::iota(suffix_array.begin(), suffix_array.end(), 0);

    // Sort suffixes lexicographically. This is a simple but slow suffix sort.
    // For large inputs, a more advanced algorithm like SA-IS would be better.
    std::sort(suffix_array.begin(), suffix_array.end(),
        [&](uint32_t a, uint32_t b) {
            const uint8_t* data_ptr = data.data();
            return std::lexicographical_compare(
                data_ptr + a, data_ptr + n,
                data_ptr + b, data_ptr + n
            );
        }
    );

    std::vector<uint8_t> last_column(n);
    uint32_t primary_index = 0;
    for (size_t i = 0; i < n; ++i) {
        last_column[i] = data[(suffix_array[i] + n - 1) % n];
        if (suffix_array[i] == 0) {
            primary_index = static_cast<uint32_t>(i);
        }
    }

    std::vector<uint8_t> compressed_data;
    compressed_data.reserve(4 + n);
    write_u32_be(compressed_data, primary_index);
    compressed_data.insert(compressed_data.end(), last_column.begin(), last_column.end());

    return compressed_data;
}

std::vector<uint8_t> BwtCompressor::inverse_bwt_transform(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    if (data.size() < 4) {
        throw std::runtime_error("Invalid BWT data: too short to contain primary index.");
    }

    size_t pos = 0;
    uint32_t primary_index = read_u32_be(data, pos);
    
    const uint8_t* last_column_ptr = data.data() + 4;
    const size_t n = data.size() - 4;

    if (n == 0) {
        return {};
    }
    if (primary_index >= n) {
         throw std::runtime_error("Invalid BWT data: primary index is out of bounds.");
    }

    std::vector<uint8_t> first_column(last_column_ptr, last_column_ptr + n);
    std::sort(first_column.begin(), first_column.end());

    std::vector<uint32_t> lf_mapping(n);
    std::vector<uint32_t> counts(256, 0);
    std::vector<uint32_t> base(256);

    for (size_t i = 0; i < n; ++i) {
        counts[first_column[i]]++;
    }

    uint32_t current_sum = 0;
    for (int i = 0; i < 256; ++i) {
        base[i] = current_sum;
        current_sum += counts[i];
    }
    
    std::fill(counts.begin(), counts.end(), 0);
    for (size_t i = 0; i < n; ++i) {
        uint8_t c = last_column_ptr[i];
        lf_mapping[base[c] + counts[c]] = static_cast<uint32_t>(i);
        counts[c]++;
    }

    std::vector<uint8_t> decompressed_data(n);
    uint32_t current_row = primary_index;
    for (size_t i = 0; i < n; ++i) {
        decompressed_data[n - 1 - i] = first_column[current_row];
        current_row = lf_mapping[current_row];
    }

    return decompressed_data;
}

std::vector<uint8_t> BwtCompressor::mtf_encode(const std::vector<uint8_t>& data) {
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

std::vector<uint8_t> BwtCompressor::mtf_decode(const std::vector<uint8_t>& data) {
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

