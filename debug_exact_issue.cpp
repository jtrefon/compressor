#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <fstream>

// Copy the exact BWT implementation from the source to debug
std::vector<uint8_t> debug_bwt_transform(const std::vector<uint8_t>& data) {
    const size_t n = data.size();
    
    std::vector<uint32_t> suffix_array(n);
    std::iota(suffix_array.begin(), suffix_array.end(), 0);

    // Simple lexicographical sort - exact copy
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

std::vector<uint8_t> debug_inverse_bwt_transform(const std::vector<uint8_t>& data) {
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

    // Use iterative reconstruction - exact copy
    std::vector<std::string> table(n);
    
    for (size_t i = 0; i < n; ++i) {
        table[i] = "";
    }
    
    for (size_t iter = 0; iter < n; ++iter) {
        for (size_t i = 0; i < n; ++i) {
            table[i] = static_cast<char>(data_ptr[i]) + table[i];
        }
        std::sort(table.begin(), table.end());
    }
    
    std::string result = table[primary_index];
    std::vector<uint8_t> decompressed_data(result.begin(), result.end());

    return decompressed_data;
}

void debugSpecificCase() {
    // Test the exact failing case
    std::ifstream file("../data/test.txt", std::ios::binary);
    std::vector<uint8_t> data(90);
    file.read(reinterpret_cast<char*>(data.data()), 90);
    
    std::cout << "=== DEBUGGING EXACT FAILING CASE (90 bytes) ===" << std::endl;
    std::cout << "Original first 20 bytes: ";
    for (size_t i = 0; i < 20; ++i) {
        std::cout << static_cast<char>(data[i]);
    }
    std::cout << std::endl;
    
    try {
        auto transformed = debug_bwt_transform(data);
        std::cout << "Transformed size: " << transformed.size() << std::endl;
        
        uint32_t primary_index = (static_cast<uint32_t>(transformed[0]) << 24) |
                               (static_cast<uint32_t>(transformed[1]) << 16) |
                               (static_cast<uint32_t>(transformed[2]) << 8) |
                               static_cast<uint32_t>(transformed[3]);
        
        std::cout << "Primary index: " << primary_index << std::endl;
        std::cout << "Last column first 20: ";
        for (size_t i = 4; i < std::min(size_t(24), transformed.size()); ++i) {
            std::cout << static_cast<char>(transformed[i]) << " ";
        }
        std::cout << std::endl;
        
        auto restored = debug_inverse_bwt_transform(transformed);
        std::cout << "Restored first 20 bytes: ";
        for (size_t i = 0; i < 20; ++i) {
            std::cout << static_cast<char>(restored[i]);
        }
        std::cout << std::endl;
        
        if (data == restored) {
            std::cout << "✅ EXACT CASE WORKS!" << std::endl;
        } else {
            std::cout << "❌ EXACT CASE FAILS!" << std::endl;
            for (size_t i = 0; i < data.size(); ++i) {
                if (data[i] != restored[i]) {
                    std::cout << "First mismatch at byte " << i << ": expected " 
                              << static_cast<int>(data[i]) << ", got " 
                              << static_cast<int>(restored[i]) << std::endl;
                    break;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ ERROR: " << e.what() << std::endl;
    }
}

int main() {
    debugSpecificCase();
    return 0;
}
