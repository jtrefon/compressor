#include <compression/BwtCompressor.hpp>
#include <iostream>
#include <vector>

int main() {
    compression::BwtCompressor compressor;
    
    // Test with small data using the full BWT pipeline
    std::vector<uint8_t> test_data = {'b', 'a', 'n', 'a', 'n', 'a', ' ', 'b', 'a', 'n', 'a', 'n', 'a', 's'};
    
    std::cout << "Original data size: " << test_data.size() << " bytes" << std::endl;
    std::cout << "Original data: ";
    for (uint8_t byte : test_data) {
        std::cout << static_cast<char>(byte);
    }
    std::cout << std::endl;
    
    try {
        auto compressed = compressor.compress(test_data);
        std::cout << "Compressed size: " << compressed.size() << " bytes" << std::endl;
        std::cout << "Compression ratio: " << (100.0 * compressed.size() / test_data.size()) << "%" << std::endl;
        
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
            std::cout << "Expected " << test_data.size() << " bytes, got " << decompressed.size() << " bytes" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Error: " << e.what() << std::endl;
    }
    
    return 0;
}
