#include <compression/BwtCompressor.hpp>
#include <iostream>
#include <fstream>
#include <vector>

std::vector<uint8_t> readFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filePath);
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
         throw std::runtime_error("Error reading file: " + filePath);
    }
    return buffer;
}

int main() {
    try {
        // Read first 1000 bytes of the test file
        std::ifstream file("../data/test.txt", std::ios::binary);
        if (!file) {
            std::cout << "Cannot open test file" << std::endl;
            return 1;
        }
        
        std::vector<uint8_t> data(1000);
        file.read(reinterpret_cast<char*>(data.data()), 1000);
        size_t bytesRead = file.gcount();
        data.resize(bytesRead);
        
        std::cout << "Testing with " << data.size() << " bytes from real data" << std::endl;
        
        compression::BwtCompressor compressor;
        
        auto compressed = compressor.compress(data);
        std::cout << "Compressed size: " << compressed.size() << " bytes" << std::endl;
        std::cout << "Compression ratio: " << (100.0 * compressed.size() / data.size()) << "%" << std::endl;
        
        auto decompressed = compressor.decompress(compressed);
        std::cout << "Decompressed size: " << decompressed.size() << " bytes" << std::endl;
        
        if (data == decompressed) {
            std::cout << "✅ REAL DATA TEST PASSED!" << std::endl;
        } else {
            std::cout << "❌ REAL DATA TEST FAILED!" << std::endl;
            std::cout << "Expected " << data.size() << " bytes, got " << decompressed.size() << " bytes" << std::endl;
            
            // Check first 100 bytes
            bool match = true;
            for (size_t i = 0; i < std::min(size_t(100), data.size()); ++i) {
                if (data[i] != decompressed[i]) {
                    std::cout << "First mismatch at byte " << i << ": expected " 
                              << static_cast<int>(data[i]) << ", got " 
                              << static_cast<int>(decompressed[i]) << std::endl;
                    match = false;
                    break;
                }
            }
            if (match) {
                std::cout << "First 100 bytes match, issue might be later" << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ ERROR: " << e.what() << std::endl;
    }
    
    return 0;
}
