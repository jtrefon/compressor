#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <iomanip>
#include <filesystem>
#include <signal.h>
#include <unistd.h>

// Include compressor headers
#include <compression/ICompressor.hpp>
#include <compression/NullCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/BwtCompressor.hpp>

// Global flag for timeout
volatile bool timeout_triggered = false;

void timeout_handler(int sig) {
    timeout_triggered = true;
    std::cout << "\n⏰ TIMEOUT TRIGGERED - Algorithm is too slow!" << std::endl;
}

// Reads a whole file into a byte vector
std::vector<uint8_t> readFile(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filePath.string());
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
         throw std::runtime_error("Error reading file: " + filePath.string());
    }
    return buffer;
}

// Test a single algorithm with timeout
void testAlgorithm(const std::string& name, 
                  std::unique_ptr<compression::ICompressor> compressor,
                  const std::vector<uint8_t>& originalData,
                  int timeoutSeconds = 30) {
    
    std::cout << "\n🧪 Testing " << name << " (timeout: " << timeoutSeconds << "s)..." << std::endl;
    
    // Set up timeout
    timeout_triggered = false;
    signal(SIGALRM, timeout_handler);
    alarm(timeoutSeconds);
    
    try {
        // --- Test Compression ---
        auto startCompress = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> compressedData;
        
        std::cout << "  📦 Compressing " << originalData.size() << " bytes..." << std::flush;
        compressedData = compressor->compress(originalData);
        
        if (timeout_triggered) {
            std::cout << " ❌ TIMEOUT during compression!" << std::endl;
            return;
        }
        
        auto endCompress = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> compressDuration = endCompress - startCompress;
        
        std::cout << " ✅ Done in " << compressDuration.count() << " ms" << std::endl;
        std::cout << "  📊 Size: " << compressedData.size() << " bytes (" 
                  << std::fixed << std::setprecision(1) 
                  << (100.0 * compressedData.size() / originalData.size()) << "%)" << std::endl;
        
        // --- Test Decompression ---
        alarm(timeoutSeconds);
        auto startDecompress = std::chrono::high_resolution_clock::now();
        
        std::cout << "  📂 Decompressing..." << std::flush;
        auto decompressedData = compressor->decompress(compressedData);
        
        if (timeout_triggered) {
            std::cout << " ❌ TIMEOUT during decompression!" << std::endl;
            return;
        }
        
        auto endDecompress = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> decompressDuration = endDecompress - startDecompress;
        
        std::cout << " ✅ Done in " << decompressDuration.count() << " ms" << std::endl;
        
        // --- Verify correctness ---
        if (decompressedData == originalData) {
            std::cout << "  ✅ Data integrity check PASSED" << std::endl;
        } else {
            std::cout << "  ❌ Data integrity check FAILED!" << std::endl;
        }
        
        std::cout << "  🎯 TOTAL TIME: " << (compressDuration.count() + decompressDuration.count()) << " ms" << std::endl;
        
    } catch (const std::exception& e) {
        if (timeout_triggered) {
            std::cout << " ❌ TIMEOUT - Exception: " << e.what() << std::endl;
        } else {
            std::cout << " ❌ ERROR: " << e.what() << std::endl;
        }
    }
    
    alarm(0); // Cancel timeout
}

int main() {
    std::cout << "🔍 PERFORMANCE DEBUG BENCHMARK" << std::endl;
    std::cout << "===============================" << std::endl;
    
    // --- Get Data File Path ---
    std::filesystem::path dataFilePath = "../data/test.txt";

    if (!std::filesystem::exists(dataFilePath)) {
         std::cerr << "Error: Benchmark data file not found at: " << dataFilePath << std::endl;
         return 1;
    }

    // --- Read Data ---
    std::vector<uint8_t> originalData;
    try {
        originalData = readFile(dataFilePath);
    } catch (const std::exception& e) {
        std::cerr << "Failed to read benchmark data: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "📁 File: " << dataFilePath << std::endl;
    std::cout << "📏 Size: " << originalData.size() << " bytes (" 
              << std::fixed << std::setprecision(1) 
              << (originalData.size() / 1024.0 / 1024.0) << " MB)" << std::endl;

    // --- Test Algorithms Individually ---
    
    // Fast algorithms first
    testAlgorithm("Null", std::make_unique<compression::NullCompressor>(), originalData, 5);
    testAlgorithm("Huffman", std::make_unique<compression::HuffmanCompressor>(), originalData, 30);
    testAlgorithm("LZ77", std::make_unique<compression::Lz77Compressor>(32768, 3, 258, false, true, true), originalData, 30);
    
    // Slow algorithms with longer timeout
    testAlgorithm("BWT", std::make_unique<compression::BwtCompressor>(), originalData, 60);

    std::cout << "\n🏁 PERFORMANCE DEBUG COMPLETE" << std::endl;
    return 0;
}
