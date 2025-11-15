#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>

void printSuffixes(const std::vector<uint8_t>& data) {
    const size_t n = data.size();
    std::cout << "All rotations of the string:" << std::endl;
    
    std::vector<std::string> rotations;
    for (size_t i = 0; i < n; ++i) {
        std::string rotation;
        for (size_t j = 0; j < n; ++j) {
            rotation += static_cast<char>(data[(i + j) % n]);
        }
        rotations.push_back(rotation);
        std::cout << i << ": " << rotation << std::endl;
    }
    
    std::cout << "\nSorted rotations:" << std::endl;
    std::sort(rotations.begin(), rotations.end());
    for (size_t i = 0; i < rotations.size(); ++i) {
        std::cout << i << ": " << rotations[i] << std::endl;
    }
    
    std::cout << "\nLast column: ";
    for (const auto& rot : rotations) {
        std::cout << rot.back() << " ";
    }
    std::cout << std::endl;
    
    // Find original string
    std::string original(data.begin(), data.end());
    auto it = std::find(rotations.begin(), rotations.end(), original);
    if (it != rotations.end()) {
        size_t index = std::distance(rotations.begin(), it);
        std::cout << "Original string at index: " << index << std::endl;
    }
}

std::vector<uint8_t> simple_bwt_transform(const std::vector<uint8_t>& data) {
    const size_t n = data.size();
    
    // Create all rotations
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
    
    // Find original string index
    std::string original(data.begin(), data.end());
    size_t primary_index = std::distance(rotations.begin(), 
        std::find(rotations.begin(), rotations.end(), original));
    
    // Extract last column
    std::vector<uint8_t> last_column;
    for (const auto& rot : rotations) {
        last_column.push_back(rot.back());
    }
    
    // Combine
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>((primary_index >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((primary_index >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((primary_index >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(primary_index & 0xFF));
    result.insert(result.end(), last_column.begin(), last_column.end());
    
    return result;
}

std::vector<uint8_t> simple_inverse_bwt_transform(const std::vector<uint8_t>& data) {
    if (data.size() < 4) throw std::runtime_error("Too short");
    
    uint32_t primary_index = (static_cast<uint32_t>(data[0]) << 24) |
                           (static_cast<uint32_t>(data[1]) << 16) |
                           (static_cast<uint32_t>(data[2]) << 8) |
                           static_cast<uint32_t>(data[3]);
    
    const uint8_t* last_col = data.data() + 4;
    const size_t n = data.size() - 4;
    
    std::cout << "Primary index: " << primary_index << std::endl;
    std::cout << "Last column: ";
    for (size_t i = 0; i < n; ++i) {
        std::cout << static_cast<char>(last_col[i]) << " ";
    }
    std::cout << std::endl;
    
    // Reconstruct using the standard BWT inverse algorithm
    std::vector<std::string> table;
    table.resize(n);
    
    // Initialize with empty strings
    for (size_t i = 0; i < n; ++i) {
        table[i] = "";
    }
    
    // Iteratively reconstruct
    for (size_t iter = 0; iter < n; ++iter) {
        std::cout << "\nIteration " << iter << ":" << std::endl;
        
        // Add last column to front
        for (size_t i = 0; i < n; ++i) {
            table[i] = static_cast<char>(last_col[i]) + table[i];
        }
        
        // Sort
        std::sort(table.begin(), table.end());
        
        // Print current state
        for (size_t i = 0; i < n; ++i) {
            std::cout << i << ": " << table[i] << std::endl;
        }
    }
    
    std::cout << "\nFinal result at index " << primary_index << ": " << table[primary_index] << std::endl;
    
    std::vector<uint8_t> result(table[primary_index].begin(), table[primary_index].end());
    return result;
}

int main() {
    std::vector<uint8_t> test = {'a', 'b', 'c', 'a', 'b', 'c'};
    
    std::cout << "=== DETAILED BWT ANALYSIS ===" << std::endl;
    printSuffixes(test);
    
    std::cout << "\n=== SIMPLE BWT TEST ===" << std::endl;
    
    try {
        auto transformed = simple_bwt_transform(test);
        std::cout << "Transformed size: " << transformed.size() << std::endl;
        
        auto restored = simple_inverse_bwt_transform(transformed);
        std::cout << "Restored: ";
        for (uint8_t b : restored) std::cout << static_cast<char>(b);
        std::cout << std::endl;
        
        if (test == restored) {
            std::cout << "✅ SIMPLE BWT WORKS!" << std::endl;
        } else {
            std::cout << "❌ SIMPLE BWT FAILED!" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ ERROR: " << e.what() << std::endl;
    }
    
    return 0;
}
