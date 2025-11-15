#include <compression/BwtCompressor.hpp>
#include <iostream>
#include <fstream>
#include <vector>

int main() {
    std::cout << "=== DEBUGGING LARGE FILE BWT ===" << std::endl;
    
    // Test progressively larger chunks to find exact failure point
    std::ifstream file("../data/test.txt", std::ios::binary);
    
    std::vector<size_t> test_sizes = {1000, 10000, 50000, 100000, 500000, 1000000};
    
    for (size_t size : test_sizes) {
        std::vector<uint8_t> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        size_t bytesRead = file.gcount();
        data.resize(bytesRead);
        
        std::cout << "\nTesting size: " << data.size() << " bytes" << std::endl;
        
        try {
            compression::BwtCompressor compressor;
            
            auto compressed = compressor.compress(data);
            std::cout << "✅ Compression: " << compressed.size() << " bytes" << std::endl;
            
            auto decompressed = compressor.decompress(compressed);
            std::cout << "✅ Decompression: " << decompressed.size() << " bytes" << std::endl;
            
            if (data == decompressed) {
                std::cout << "✅ SUCCESS: Perfect match!" << std::endl;
            } else {
                std::cout << "❌ FAILURE: Data mismatch!" << std::endl;
                
                // Find first mismatch
                size_t mismatches = 0;
                for (size_t i = 0; i < std::min(data.size(), decompressed.size()); ++i) {
                    if (data[i] != decompressed[i]) {
                        mismatches++;
                        if (mismatches <= 5) {
                            std::cout << "   Mismatch at " << i << ": expected " 
                                      << static_cast<int>(data[i]) << ", got " 
                                      << static_cast<int>(decompressed[i]) << std::endl;
                        }
                    }
                }
                std::cout << "   Total mismatches: " << mismatches << std::endl;
                break;
            }
            
        } catch (const std::exception& e) {
            std::cout << "❌ ERROR: " << e.what() << std::endl;
            break;
        }
        
        // Reset file position for next test
        file.clear();
        file.seekg(0, std::ios::beg);
    }
    
    return 0;
}
