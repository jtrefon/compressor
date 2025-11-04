#include <compression/ArithmeticCompressor.hpp>
#include <iostream>
#include <vector>

int main() {
    compression::ArithmeticCompressor compressor;
    
    // Test with simple data
    std::vector<uint8_t> test_data = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    
    std::cout << "Original data size: " << test_data.size() << " bytes" << std::endl;
    std::cout << "Original data: ";
    for (uint8_t byte : test_data) {
        std::cout << static_cast<char>(byte);
    }
    std::cout << std::endl;
    
    try {
        auto compressed = compressor.compress(test_data);
        std::cout << "Compressed size: " << compressed.size() << " bytes" << std::endl;
        
        auto decompressed = compressor.decompress(compressed);
        std::cout << "Decompressed size: " << decompressed.size() << " bytes" << std::endl;
        
        std::cout << "Decompressed data: ";
        for (uint8_t byte : decompressed) {
            std::cout << static_cast<char>(byte);
        }
        std::cout << std::endl;
        
        if (test_data == decompressed) {
            std::cout << "✅ Test PASSED - data matches!" << std::endl;
        } else {
            std::cout << "❌ Test FAILED - data mismatch!" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Error: " << e.what() << std::endl;
    }
    
    return 0;
}
