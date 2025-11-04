#include <compression/EnhancedBwtCompressor.hpp>
#include <compression/ArithmeticCompressor.hpp>
#include <algorithm>
#include <numeric>

namespace compression {

EnhancedBwtCompressor::EnhancedBwtCompressor() 
    : arithmetic_(std::make_unique<ArithmeticCompressor>()) {
}

EnhancedBwtCompressor::~EnhancedBwtCompressor() = default;

std::vector<uint8_t> EnhancedBwtCompressor::compress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    // Use existing BWT transform but replace Huffman with Arithmetic coding
    const auto bwt_transformed = bwt_transform(data);
    const auto mtf_encoded = mtf_encode(bwt_transformed);
    const auto rle_encoded = rle_encode(mtf_encoded);
    
    // Use arithmetic coding instead of Huffman for better compression
    return arithmetic_->compress(rle_encoded);
}

std::vector<uint8_t> EnhancedBwtCompressor::decompress(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }
    
    try {
        // Reverse the compression stages using arithmetic decoder
        const auto rle_decoded = arithmetic_->decompress(data);
        const auto mtf_decoded = mtf_decode(rle_decoded);
        const auto bwt_decoded = bwt_inverse_transform(mtf_decoded);
        
        return bwt_decoded;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Enhanced BWT decompression failed: ") + e.what());
    }
}

// Copy the BWT transform methods from the original BwtCompressor
std::vector<uint8_t> EnhancedBwtCompressor::bwt_transform(const std::vector<uint8_t>& data) const {
    const size_t n = data.size();
    std::vector<uint32_t> suffix_array(n);
    std::iota(suffix_array.begin(), suffix_array.end(), 0);

    // Use the optimized suffix array construction
    std::vector<uint32_t> rank(n);
    std::vector<uint32_t> new_rank(n);
    
    // Initial ranking based on single characters
    for (size_t i = 0; i < n; ++i) {
        rank[i] = data[i];
    }
    
    // Optimized radix sort based on 2k substrings, doubling k each iteration
    for (size_t k = 1; k < n; k <<= 1) {
        // Use counting sort for better performance on character data
        const size_t max_rank = n + 256;
        
        // Sort by second key using counting sort
        std::vector<size_t> count(max_rank, 0);
        std::vector<uint32_t> temp_suffix(n);
        
        // Count frequencies of second keys
        for (size_t i = 0; i < n; ++i) {
            uint32_t second_key = (suffix_array[i] + k < n) ? rank[suffix_array[i] + k] : 0;
            count[second_key + 1]++;
        }
        
        // Compute prefix sums
        for (size_t i = 1; i < max_rank; ++i) {
            count[i] += count[i - 1];
        }
        
        // Place elements in correct positions
        for (size_t i = 0; i < n; ++i) {
            uint32_t second_key = (suffix_array[i] + k < n) ? rank[suffix_array[i] + k] : 0;
            temp_suffix[count[second_key]++] = suffix_array[i];
        }
        
        suffix_array = temp_suffix;
        
        // Sort by first key using counting sort
        std::fill(count.begin(), count.end(), 0);
        
        // Count frequencies of first keys
        for (size_t i = 0; i < n; ++i) {
            uint32_t first_key = rank[suffix_array[i]];
            count[first_key + 1]++;
        }
        
        // Compute prefix sums
        for (size_t i = 1; i < max_rank; ++i) {
            count[i] += count[i - 1];
        }
        
        // Place elements in correct positions
        for (size_t i = 0; i < n; ++i) {
            uint32_t first_key = rank[suffix_array[i]];
            temp_suffix[count[first_key]++] = suffix_array[i];
        }
        
        suffix_array = temp_suffix;
        
        // Update rankings
        new_rank[suffix_array[0]] = 0;
        for (size_t i = 1; i < n; ++i) {
            uint32_t a = suffix_array[i-1];
            uint32_t b = suffix_array[i];
            
            uint32_t a_second = (a + k < n) ? rank[a + k] : 0;
            uint32_t b_second = (b + k < n) ? rank[b + k] : 0;
            
            if (rank[a] == rank[b] && a_second == b_second) {
                new_rank[b] = new_rank[a];
            } else {
                new_rank[b] = new_rank[a] + 1;
            }
        }
        
        rank.swap(new_rank);
        
        // Early termination if all suffixes have unique ranks
        if (rank[suffix_array[n-1]] == n-1) break;
    }

    std::vector<uint8_t> last_column(n);
    uint32_t primary_index = 0;
    for (size_t i = 0; i < n; ++i) {
        last_column[i] = data[(suffix_array[i] + n - 1) % n];
        if (suffix_array[i] == 0) {
            primary_index = static_cast<uint32_t>(i);
        }
    }

    // Prepend primary index to the transformed data
    std::vector<uint8_t> result;
    result.reserve(4 + n);
    // Write primary index in big-endian format
    result.push_back(static_cast<uint8_t>((primary_index >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((primary_index >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((primary_index >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(primary_index & 0xFF));
    
    // Append the last column
    result.insert(result.end(), last_column.begin(), last_column.end());

    return result;
}

std::vector<uint8_t> EnhancedBwtCompressor::bwt_inverse_transform(const std::vector<uint8_t>& data) const {
    const size_t n = data.size() - 4; // Subtract 4 bytes for primary index
    if (n == 0) {
        return {};
    }

    // Read primary index from first 4 bytes (big-endian)
    uint32_t primary_index = (static_cast<uint32_t>(data[0]) << 24) |
                             (static_cast<uint32_t>(data[1]) << 16) |
                             (static_cast<uint32_t>(data[2]) << 8)  |
                             static_cast<uint32_t>(data[3]);

    const std::vector<uint8_t> last_column(data.begin() + 4, data.end());

    // Build the table of occurrences for each character
    std::array<std::vector<uint32_t>, 256> table;
    std::array<uint32_t, 256> char_count{};
    
    // First pass: count occurrences of each character
    for (uint8_t c : last_column) {
        char_count[c]++;
    }
    
    // Second pass: build the table
    std::array<uint32_t, 256> char_start{};
    for (int i = 1; i < 256; ++i) {
        char_start[i] = char_start[i-1] + char_count[i-1];
    }
    
    // Initialize table vectors
    for (int i = 0; i < 256; ++i) {
        table[i].resize(char_count[i]);
    }
    
    // Fill the table
    std::array<uint32_t, 256> current_pos{};
    for (size_t i = 0; i < n; ++i) {
        uint8_t c = last_column[i];
        table[c][current_pos[c]++] = static_cast<uint32_t>(i);
    }

    // Reconstruct the original string
    std::vector<uint8_t> result(n);
    uint32_t current_row = primary_index;
    
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        uint8_t c = last_column[current_row];
        result[i] = c;
        
        // Find the position in the table
        uint32_t pos_in_table = 0;
        for (size_t j = 0; j < table[c].size(); ++j) {
            if (table[c][j] == current_row) {
                pos_in_table = static_cast<uint32_t>(j);
                break;
            }
        }
        
        current_row = char_start[c] + pos_in_table;
    }

    return result;
}

// Copy MTF encoding methods
std::vector<uint8_t> EnhancedBwtCompressor::mtf_encode(const std::vector<uint8_t>& data) const {
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

std::vector<uint8_t> EnhancedBwtCompressor::mtf_decode(const std::vector<uint8_t>& data) const {
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

// Copy RLE encoding methods  
std::vector<uint8_t> EnhancedBwtCompressor::rle_encode(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }

    std::vector<uint8_t> encoded;
    encoded.reserve(data.size() * 2); // Worst case estimate

    for (size_t i = 0; i < data.size(); ) {
        uint8_t current = data[i];
        size_t count = 1;
        
        // Count consecutive identical bytes
        while (i + count < data.size() && data[i + count] == current && count < 255) {
            count++;
        }
        
        if (count > 1 || current == 0) {
            // Encode as [count][value] for runs or zero bytes
            encoded.push_back(static_cast<uint8_t>(count));
            encoded.push_back(current);
        } else {
            // Encode single non-zero bytes as [0][value]
            encoded.push_back(0);
            encoded.push_back(current);
        }
        
        i += count;
    }

    return encoded;
}

std::vector<uint8_t> EnhancedBwtCompressor::rle_decode(const std::vector<uint8_t>& data) const {
    if (data.empty()) {
        return {};
    }

    if (data.size() % 2 != 0) {
        throw std::runtime_error("Invalid RLE data: length must be even");
    }

    std::vector<uint8_t> decoded;
    decoded.reserve(data.size() * 2); // Estimate

    for (size_t i = 0; i < data.size(); i += 2) {
        uint8_t count = data[i];
        uint8_t value = data[i + 1];
        
        if (count == 0) {
            // Single byte
            decoded.push_back(value);
        } else {
            // Run of bytes
            decoded.insert(decoded.end(), count, value);
        }
    }

    return decoded;
}

} // namespace compression
