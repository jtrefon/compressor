#include <compression/BwtCompressor.hpp>
#include <iostream>
#include <fstream>
#include <vector>

void testChunkSize(size_t chunkSize) {
    std::ifstream file("../data/test.txt", std::ios::binary);
    if (!file) {
        std::cout << "Cannot open test file" << std::endl;
        return;
    }
    
    std::vector<uint8_t> data(chunkSize);
    file.read(reinterpret_cast<char*>(data.data()), chunkSize);
    size_t bytesRead = file.gcount();
    data.resize(bytesRead);
    
    std::cout << "Testing chunk size " << data.size() << " bytes... ";
    
    try {
        compression::BwtCompressor compressor;
        
        auto compressed = compressor.compress(data);
        auto decompressed = compressor.decompress(compressed);
        
        if (data == decompressed) {
            std::cout << "✅ PASSED" << std::endl;
        } else {
            std::cout << "❌ FAILED" << std::endl;
            
            // Find first mismatch
            for (size_t i = 0; i < data.size(); ++i) {
                if (data[i] != decompressed[i]) {
                    std::cout << "   First mismatch at byte " << i << std::endl;
                    break;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ ERROR: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=== TESTING DIFFERENT CHUNK SIZES ===" << std::endl;
    
    // Test various chunk sizes to find the boundary
    testChunkSize(50);
    testChunkSize(80);
    testChunkSize(90);
    testChunkSize(100);
    testChunkSize(128);
    testChunkSize(256);
    testChunkSize(512);
    testChunkSize(1000);
    testChunkSize(2000);
    testChunkSize(5000);
    testChunkSize(10000);
    testChunkSize(50000);
    testChunkSize(100000);
    testChunkSize(500000);
    
    return 0;
}
