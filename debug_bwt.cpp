#include <compression/BwtCompressor.hpp>
#include <iostream>
#include <vector>
#include <iomanip>

void printHex(const std::vector<uint8_t>& data, const std::string& label) {
    std::cout << label << " (" << data.size() << " bytes): ";
    for (size_t i = 0; i < std::min(size_t(32), data.size()); ++i) {
        printf("%02x ", data[i]);
    }
    if (data.size() > 32) std::cout << "...";
    std::cout << std::endl;
}

void printText(const std::vector<uint8_t>& data, const std::string& label) {
    std::cout << label << " (" << data.size() << " bytes): ";
    for (size_t i = 0; i < std::min(size_t(64), data.size()); ++i) {
        if (data[i] >= 32 && data[i] <= 126) {
            std::cout << static_cast<char>(data[i]);
        } else {
            std::cout << ".";
        }
    }
    if (data.size() > 64) std::cout << "...";
    std::cout << std::endl;
}

int main() {
    compression::BwtCompressor compressor;
    
    // Test with very simple data first
    std::vector<uint8_t> simple = {'a', 'b', 'c', 'a', 'b', 'c'};
    
    std::cout << "=== SIMPLE TEST ===" << std::endl;
    printText(simple, "Original");
    
    try {
        auto compressed = compressor.compress(simple);
        printHex(compressed, "Compressed");
        
        auto decompressed = compressor.decompress(compressed);
        printText(decompressed, "Decompressed");
        
        if (simple == decompressed) {
            std::cout << "✅ SIMPLE TEST PASSED" << std::endl;
        } else {
            std::cout << "❌ SIMPLE TEST FAILED" << std::endl;
            std::cout << "Expected: ";
            for (auto b : simple) std::cout << static_cast<char>(b);
            std::cout << std::endl;
            std::cout << "Got:      ";
            for (auto b : decompressed) std::cout << static_cast<char>(b);
            std::cout << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ SIMPLE TEST ERROR: " << e.what() << std::endl;
    }
    
    std::cout << "\n=== BANANA TEST ===" << std::endl;
    std::vector<uint8_t> banana = {'b', 'a', 'n', 'a', 'n', 'a'};
    printText(banana, "Original");
    
    try {
        auto compressed = compressor.compress(banana);
        printHex(compressed, "Compressed");
        
        auto decompressed = compressor.decompress(compressed);
        printText(decompressed, "Decompressed");
        
        if (banana == decompressed) {
            std::cout << "✅ BANANA TEST PASSED" << std::endl;
        } else {
            std::cout << "❌ BANANA TEST FAILED" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ BANANA TEST ERROR: " << e.what() << std::endl;
    }
    
    return 0;
}
