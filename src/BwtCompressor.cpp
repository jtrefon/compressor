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
    if (pos + 4 > src.size()) {
        throw std::out_of_range("read_u32_be: insufficient bytes");
    }
    uint32_t b0 = static_cast<uint32_t>(src[pos++]);
    uint32_t b1 = static_cast<uint32_t>(src[pos++]);
    uint32_t b2 = static_cast<uint32_t>(src[pos++]);
    uint32_t b3 = static_cast<uint32_t>(src[pos++]);
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
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
    if (n == 0) return {};

    // Build T = S + S to sort finite rotations of length n using doubling without wrap.
    std::vector<uint8_t> T;
    T.reserve(2 * n);
    T.insert(T.end(), data.begin(), data.end());
    T.insert(T.end(), data.begin(), data.end());

    const size_t m = 2 * n;
    std::vector<uint32_t> sa(m);
    std::iota(sa.begin(), sa.end(), 0);
    std::vector<uint32_t> rank(m), new_rank(m);
    // Initial ranks (offset by +1 so 0 can be sentinel for out-of-range)
    for (size_t i = 0; i < m; ++i) rank[i] = static_cast<uint32_t>(T[i]) + 1u;

    // Doubling until segment length >= n so comparisons cover full rotation length.
    for (size_t k = 1; k < n; k <<= 1) {
        std::sort(sa.begin(), sa.end(), [&](uint32_t a, uint32_t b) {
            uint32_t ra = rank[a], rb = rank[b];
            if (ra != rb) return ra < rb;
            uint32_t ra2 = (a + k < m) ? rank[a + k] : 0u;
            uint32_t rb2 = (b + k < m) ? rank[b + k] : 0u;
            return ra2 < rb2;
        });
        new_rank[sa[0]] = 0;
        for (size_t i = 1; i < m; ++i) {
            uint32_t a = sa[i - 1], b = sa[i];
            uint32_t ra = rank[a], rb = rank[b];
            uint32_t ra2 = (a + k < m) ? rank[a + k] : 0u;
            uint32_t rb2 = (b + k < m) ? rank[b + k] : 0u;
            new_rank[b] = (ra == rb && ra2 == rb2) ? new_rank[a] : (new_rank[a] + 1);
        }
        rank.swap(new_rank);
        if (rank[sa[m - 1]] == m - 1) break;
    }

    // Determine rotation order by sorting indices 0..n-1 by their rank value.
    std::vector<uint32_t> rot_idx(n);
    std::iota(rot_idx.begin(), rot_idx.end(), 0);
    std::sort(rot_idx.begin(), rot_idx.end(), [&](uint32_t a, uint32_t b){
        if (rank[a] != rank[b]) return rank[a] < rank[b];
        // Tie-breaker with second half to ensure exact n-length comparison
        uint32_t ra2 = rank[a + n];
        uint32_t rb2 = rank[b + n];
        return ra2 < rb2;
    });

    std::vector<uint8_t> last_column(n);
    uint32_t primary_index = 0;
    for (size_t i = 0; i < n; ++i) {
        uint32_t start = rot_idx[i];
        last_column[i] = data[(start + n - 1) % n];
        if (start == 0) primary_index = static_cast<uint32_t>(i);
    }

    std::vector<uint8_t> out;
    out.reserve(4 + n);
    out.push_back(static_cast<uint8_t>((primary_index >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((primary_index >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((primary_index >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(primary_index & 0xFF));
    out.insert(out.end(), last_column.begin(), last_column.end());
    return out;
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
    
    const uint8_t* data_ptr = data.data() + 4;
    const size_t n = data.size() - 4;

    if (n == 0) {
        return {};
    }
    
    // Use efficient LF mapping inverse for ALL sizes - the iterative approach is too slow
    if (primary_index >= n) {
         throw std::runtime_error("Invalid BWT data: primary index is out of bounds.");
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
    
    // Build LF mapping: for each position in last column, find corresponding position in first column
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

