#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <fstream>

// Use the simplest possible BWT implementation that's guaranteed to work
std::vector<uint8_t> simple_bwt_transform(const std::vector<uint8_t>& data) {
    const size_t n = data.size();
    
    // Create all rotations explicitly
    std::vector<std::string> rotations;
    for (size_t i = 0; i < n; ++i) {
        std::string rotation;
        for (size_t j = 0; j < n; ++j) {
            rotation += static_cast<char>(data[(i + j) % n]);
        }
        rotations.push_back(rotation);
    }
    
    // Sort rotations
    std::sort(rotations.begin(), rotations.end());
    
    // Find original string
    std::string original(data.begin(), data.end());
    size_t primary_index = std::distance(rotations.begin(), 
        std::find(rotations.begin(), rotations.end(), original));
    
    // Extract last column
    std::vector<uint8_t> last_column;
    for (const auto& rot : rotations) {
        last_column.push_back(rot.back());
    }
    
    // Combine with primary index
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>((primary_index >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((primary_index >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((primary_index >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(primary_index & 0xFF));
    result.insert(result.end(), last_column.begin(), last_column.end());
    
    return result;
}

std::vector<uint8_t> simple_bwt_inverse(const std::vector<uint8_t>& data) {
    if (data.size() < 4) throw std::runtime_error("Too short");
    
    uint32_t primary_index = (static_cast<uint32_t>(data[0]) << 24) |
                           (static_cast<uint32_t>(data[1]) << 16) |
                           (static_cast<uint32_t>(data[2]) << 8) |
                           static_cast<uint32_t>(data[3]);
    
    const uint8_t* last_col = data.data() + 4;
    const size_t n = data.size() - 4;
    
    // Iterative reconstruction - guaranteed to work
    std::vector<std::string> table(n);
    
    for (size_t i = 0; i < n; ++i) {
        table[i] = "";
    }
    
    for (size_t iter = 0; iter < n; ++iter) {
        for (size_t i = 0; i < n; ++i) {
            table[i] = static_cast<char>(last_col[i]) + table[i];
        }
        std::sort(table.begin(), table.end());
    }
    
    std::string result = table[primary_index];
    return std::vector<uint8_t>(result.begin(), result.end());
}

int main() {
    std::cout << "=== TESTING SIMPLE BWT ===" << std::endl;
    
    // Test with actual file data
    std::ifstream file("../data/test.txt", std::ios::binary);
    
    std::vector<size_t> test_sizes = {1000, 10000, 50000, 100000};
    
    for (size_t size : test_sizes) {
        std::vector<uint8_t> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        size_t bytesRead = file.gcount();
        data.resize(bytesRead);
        
        std::cout << "\nTesting size: " << data.size() << " bytes" << std::endl;
        
        try {
            auto transformed = simple_bwt_transform(data);
            std::cout << "✅ Transform: " << transformed.size() << " bytes" << std::endl;
            
            auto restored = simple_bwt_inverse(transformed);
            std::cout << "✅ Restore: " << restored.size() << " bytes" << std::endl;
            
            if (data == restored) {
                std::cout << "✅ SUCCESS: Perfect match!" << std::endl;
            } else {
                std::cout << "❌ FAILURE: Data mismatch!" << std::endl;
                break;
            }
            
        } catch (const std::exception& e) {
            std::cout << "❌ ERROR: " << e.what() << std::endl;
            break;
        }
        
        // Reset file position
        file.clear();
        file.seekg(0, std::ios::beg);
    }
    
    return 0;
}
