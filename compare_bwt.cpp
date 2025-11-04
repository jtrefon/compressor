#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>

// Working simple BWT from our test
std::vector<uint8_t> working_bwt_transform(const std::vector<uint8_t>& data) {
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

// Test the actual BWTCompressor class
#include <compression/BwtCompressor.hpp>

int main() {
    std::vector<uint8_t> test = {'a', 'b', 'c', 'a', 'b', 'c'};
    
    std::cout << "=== COMPARING BWT IMPLEMENTATIONS ===" << std::endl;
    std::cout << "Original: ";
    for (uint8_t b : test) std::cout << static_cast<char>(b);
    std::cout << std::endl;
    
    // Test working version
    auto working_result = working_bwt_transform(test);
    std::cout << "Working BWT: ";
    for (size_t i = 4; i < working_result.size(); ++i) {
        std::cout << static_cast<char>(working_result[i]) << " ";
    }
    std::cout << "(index: " << (static_cast<uint32_t>(working_result[0]) << 24 | 
                                   static_cast<uint32_t>(working_result[1]) << 16 | 
                                   static_cast<uint32_t>(working_result[2]) << 8 | 
                                   static_cast<uint32_t>(working_result[3])) << ")" << std::endl;
    
    // Test actual BWTCompressor (need to extract raw BWT part)
    // Let's manually call the bwt_transform method
    compression::BwtCompressor compressor;
    
    // We can't access private methods directly, so let's test the full pipeline
    auto full_compressed = compressor.compress(test);
    std::cout << "Full pipeline compressed size: " << full_compressed.size() << std::endl;
    
    // The issue might be that the full pipeline is working but our test is wrong
    auto decompressed = compressor.decompress(full_compressed);
    std::cout << "Full pipeline result: ";
    for (uint8_t b : decompressed) std::cout << static_cast<char>(b);
    std::cout << std::endl;
    
    if (test == decompressed) {
        std::cout << "✅ FULL PIPELINE WORKS!" << std::endl;
    } else {
        std::cout << "❌ FULL PIPELINE BROKEN!" << std::endl;
    }
    
    return 0;
}
