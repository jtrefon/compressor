#include <compression/OptimizedCompressor.hpp>
#include <iostream>
#include <vector>

int main() {
    compression::OptimizedCompressor compressor;
    
    // Test with simple repetitive data
    std::vector<uint8_t> test_data = {'A', 'A', 'A', 'A', 'A', 'B', 'B', 'C', 'C', 'C', 'C'};
    
    std::cout << "Original data size: " << test_data.size() << " bytes" << std::endl;
    std::cout << "Original data: ";
    for (uint8_t byte : test_data) {
        std::cout << static_cast<char>(byte) << " ";
    }
    std::cout << std::endl;
    
    try {
        auto compressed = compressor.compress(test_data);
        std::cout << "Compressed size: " << compressed.size() << " bytes" << std::endl;
        std::cout << "Compression ratio: " << (100.0 * compressed.size() / test_data.size()) << "%" << std::endl;
        
        std::cout << "Compressed data: ";
        for (uint8_t byte : compressed) {
            std::cout << std::hex << static_cast<int>(byte) << " ";
        }
        std::cout << std::dec << std::endl;
        
        auto decompressed = compressor.decompress(compressed);
        std::cout << "Decompressed size: " << decompressed.size() << " bytes" << std::endl;
        
        std::cout << "Decompressed data: ";
        for (uint8_t byte : decompressed) {
            std::cout << static_cast<char>(byte) << " ";
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
