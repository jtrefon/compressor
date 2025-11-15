#include <compression/BwtCompressor.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>

void testSize(size_t size) {
    std::ifstream file("../data/test.txt", std::ios::binary);
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    size_t bytesRead = file.gcount();
    data.resize(bytesRead);
    
    std::cout << "\n=== TESTING SIZE: " << data.size() << " bytes ===" << std::endl;
    
    try {
        compression::BwtCompressor compressor;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto compressed = compressor.compress(data);
        auto compress_end = std::chrono::high_resolution_clock::now();
        
        std::cout << "✅ Compression: " << compressed.size() << " bytes (ratio: " 
                  << (100.0 * compressed.size() / data.size()) << "%)" << std::endl;
        
        auto decompress_start = std::chrono::high_resolution_clock::now();
        auto decompressed = compressor.decompress(compressed);
        auto decompress_end = std::chrono::high_resolution_clock::now();
        
        std::chrono::duration<double, std::milli> compress_time = compress_end - start;
        std::chrono::duration<double, std::milli> decompress_time = decompress_end - decompress_start;
        
        std::cout << "⏱️  Times: " << compress_time.count() << "ms compress, " 
                  << decompress_time.count() << "ms decompress" << std::endl;
        
        if (data == decompressed) {
            std::cout << "✅ SUCCESS: Perfect round-trip!" << std::endl;
        } else {
            std::cout << "❌ FAILURE: Data mismatch!" << std::endl;
            
            // Find first mismatch
            size_t mismatch_pos = 0;
            for (size_t i = 0; i < data.size(); ++i) {
                if (i >= decompressed.size() || data[i] != decompressed[i]) {
                    mismatch_pos = i;
                    break;
                }
            }
            std::cout << "   First mismatch at position: " << mismatch_pos << std::endl;
            
            if (mismatch_pos < data.size()) {
                std::cout << "   Expected byte: " << static_cast<int>(data[mismatch_pos]) << std::endl;
                if (mismatch_pos < decompressed.size()) {
                    std::cout << "   Got byte:      " << static_cast<int>(decompressed[mismatch_pos]) << std::endl;
                } else {
                    std::cout << "   Got: END OF DATA" << std::endl;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ ERROR: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "🎯 FINAL BWT COMPREHENSIVE TEST" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Test progressively larger sizes to find the exact failure point
    std::vector<size_t> sizes = {
        1000, 5000, 10000, 25000, 50000, 75000, 100000, 
        250000, 500000, 750000, 1000000, 2500000, 5000000, 6488663
    };
    
    for (size_t size : sizes) {
        testSize(size);
        
        // If we find a failure, we can stop or continue to see the pattern
        std::cout << "Press Enter to continue to next size..." << std::endl;
        std::cin.get();
    }
    
    return 0;
}
