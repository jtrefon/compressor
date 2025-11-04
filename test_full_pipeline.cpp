#include <compression/BwtCompressor.hpp>
#include <iostream>
#include <fstream>
#include <vector>

void testFullPipeline(size_t size) {
    std::ifstream file("../data/test.txt", std::ios::binary);
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    size_t bytesRead = file.gcount();
    data.resize(bytesRead);
    
    std::cout << "=== TESTING FULL BWT PIPELINE (" << data.size() << " bytes) ===" << std::endl;
    std::cout << "Original first 20: ";
    for (size_t i = 0; i < std::min(size_t(20), data.size()); ++i) {
        std::cout << static_cast<char>(data[i]);
    }
    std::cout << std::endl;
    
    try {
        compression::BwtCompressor compressor;
        
        auto compressed = compressor.compress(data);
        std::cout << "Compressed size: " << compressed.size() << " bytes" << std::endl;
        std::cout << "Compression ratio: " << (100.0 * compressed.size() / data.size()) << "%" << std::endl;
        
        auto decompressed = compressor.decompress(compressed);
        std::cout << "Decompressed first 20: ";
        for (size_t i = 0; i < std::min(size_t(20), decompressed.size()); ++i) {
            std::cout << static_cast<char>(decompressed[i]);
        }
        std::cout << std::endl;
        
        if (data == decompressed) {
            std::cout << "✅ FULL PIPELINE WORKS!" << std::endl;
        } else {
            std::cout << "❌ FULL PIPELINE FAILS!" << std::endl;
            for (size_t i = 0; i < data.size(); ++i) {
                if (data[i] != decompressed[i]) {
                    std::cout << "First mismatch at byte " << i << ": expected " 
                              << static_cast<int>(data[i]) << ", got " 
                              << static_cast<int>(decompressed[i]) << std::endl;
                    break;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ ERROR: " << e.what() << std::endl;
    }
}

int main() {
    testFullPipeline(90);
    testFullPipeline(100);
    testFullPipeline(256);
    testFullPipeline(1000);
    
    return 0;
}
