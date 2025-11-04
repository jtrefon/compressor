#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>

// Raw BWT implementation for testing
std::vector<uint8_t> raw_bwt_transform(const std::vector<uint8_t>& data) {
    const size_t n = data.size();
    
    std::vector<uint32_t> suffix_array(n);
    std::iota(suffix_array.begin(), suffix_array.end(), 0);

    // Simple lexicographical sort for testing
    std::sort(suffix_array.begin(), suffix_array.end(), 
        [&data, n](uint32_t a, uint32_t b) {
            for (size_t i = 0; i < n; ++i) {
                uint8_t ca = data[(a + i) % n];
                uint8_t cb = data[(b + i) % n];
                if (ca != cb) return ca < cb;
            }
            return a < b;
        });

    std::vector<uint8_t> last_column(n);
    uint32_t primary_index = 0;
    for (size_t i = 0; i < n; ++i) {
        last_column[i] = data[(suffix_array[i] + n - 1) % n];
        if (suffix_array[i] == 0) {
            primary_index = static_cast<uint32_t>(i);
        }
    }

    // Prepend primary index
    std::vector<uint8_t> result;
    result.reserve(4 + n);
    result.push_back(static_cast<uint8_t>((primary_index >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((primary_index >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((primary_index >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(primary_index & 0xFF));
    result.insert(result.end(), last_column.begin(), last_column.end());

    return result;
}

std::vector<uint8_t> raw_inverse_bwt_transform(const std::vector<uint8_t>& data) {
    if (data.empty()) return {};
    if (data.size() < 4) throw std::runtime_error("Too short");
    
    // Read primary index
    uint32_t primary_index = (static_cast<uint32_t>(data[0]) << 24) |
                           (static_cast<uint32_t>(data[1]) << 16) |
                           (static_cast<uint32_t>(data[2]) << 8) |
                           static_cast<uint32_t>(data[3]);
    
    const uint8_t* data_ptr = data.data() + 4;
    const size_t n = data.size() - 4;

    if (n == 0) return {};
    if (primary_index >= n) throw std::runtime_error("Invalid primary index");

    std::vector<uint8_t> first_column(data_ptr, data_ptr + n);
    std::sort(first_column.begin(), first_column.end());

    std::vector<uint32_t> lf_mapping(n);
    std::vector<uint32_t> counts(256, 0);
    
    // Count frequencies
    for (size_t i = 0; i < n; ++i) {
        counts[first_column[i]]++;
    }

    std::vector<uint32_t> base(256);
    uint32_t current_sum = 0;
    for (int i = 0; i < 256; ++i) {
        base[i] = current_sum;
        current_sum += counts[i];
    }
    
    std::fill(counts.begin(), counts.end(), 0);
    for (size_t i = 0; i < n; ++i) {
        uint8_t c = data_ptr[i];
        lf_mapping[base[c] + counts[c]] = static_cast<uint32_t>(i);
        counts[c]++;
    }

    std::vector<uint8_t> result(n);
    uint32_t current_row = primary_index;
    for (size_t i = 0; i < n; ++i) {
        result[n - 1 - i] = first_column[current_row];
        current_row = lf_mapping[current_row];
    }
    
    return result;
}

void printData(const std::vector<uint8_t>& data, const std::string& label) {
    std::cout << label << " (" << data.size() << "): ";
    for (uint8_t b : data) {
        if (b >= 32 && b <= 126) std::cout << static_cast<char>(b);
        else std::cout << ".";
    }
    std::cout << std::endl;
}

int main() {
    std::vector<uint8_t> test = {'a', 'b', 'c', 'a', 'b', 'c'};
    printData(test, "Original");
    
    try {
        auto transformed = raw_bwt_transform(test);
        printData(transformed, "BWT Transformed");
        
        auto restored = raw_inverse_bwt_transform(transformed);
        printData(restored, "Restored");
        
        if (test == restored) {
            std::cout << "✅ RAW BWT WORKS!" << std::endl;
        } else {
            std::cout << "❌ RAW BWT FAILED!" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ ERROR: " << e.what() << std::endl;
    }
    
    return 0;
}
