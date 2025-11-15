#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <iomanip>
#include <filesystem>

// Include all compressors including the new ultra ones
#include <compression/ICompressor.hpp>
#include <compression/NullCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <compression/UltraCompressor.hpp>
#include <compression/ExtremeCompressor.hpp>

// Structure to hold benchmark results
struct BenchmarkResult {
    std::string algorithmName;
    size_t originalSize = 0;
    size_t compressedSize = 0;
    double compressionTimeMs = 0.0;
    double decompressionTimeMs = 0.0;
    double ratio = 0.0;
    bool success = true;
    std::string errorReason;
};

// Safe file reading
std::vector<uint8_t> readFile(const std::filesystem::path& filePath, size_t maxSize = SIZE_MAX) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + filePath.string());
    }
    std::streamsize size = file.tellg();
    size = std::min(size, static_cast<std::streamsize>(maxSize));
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
         throw std::runtime_error("Error reading file: " + filePath.string());
    }
    return buffer;
}

// Ultra benchmark function with progress tracking
BenchmarkResult runUltraBenchmark(
    const std::string& name,
    std::unique_ptr<compression::ICompressor> compressor,
    const std::vector<uint8_t>& originalData)
{
    BenchmarkResult result;
    result.algorithmName = name;
    result.originalSize = originalData.size();

    if (originalData.empty()) {
        return result;
    }

    std::cout << "🚀 Testing " << name << " (" << originalData.size() << " bytes)..." << std::flush;

    try {
        // --- Time Compression ---
        auto startCompress = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> compressedData = compressor->compress(originalData);
        auto endCompress = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> compressDuration = endCompress - startCompress;
        result.compressionTimeMs = compressDuration.count();
        result.compressedSize = compressedData.size();

        std::cout << " 📦" << std::flush;

        // --- Time Decompression ---
        if (!compressedData.empty()) {
            try {
                auto startDecompress = std::chrono::high_resolution_clock::now();
                auto decompressedData = compressor->decompress(compressedData);
                auto endDecompress = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> decompressDuration = endDecompress - startDecompress;
                result.decompressionTimeMs = decompressDuration.count();

                std::cout << " 📂" << std::flush;

                // Sanity check decompression
                if (decompressedData != originalData) {
                    std::cout << " ❌ MISMATCH!" << std::endl;
                    result.success = false;
                    result.errorReason = "Decompression mismatch";
                    
                    // Show first mismatch
                    for (size_t i = 0; i < originalData.size(); ++i) {
                        if (i >= decompressedData.size() || originalData[i] != decompressedData[i]) {
                            std::cout << "   First mismatch at byte " << i << std::endl;
                            break;
                        }
                    }
                } else {
                    std::cout << " ✅" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << " ❌ DECOMPRESS ERROR: " << e.what() << std::endl;
                result.success = false;
                result.errorReason = std::string("Decompression error: ") + e.what();
                result.decompressionTimeMs = std::numeric_limits<double>::infinity();
            }
        }
    } catch (const std::exception& e) {
        std::cout << " ❌ COMPRESS ERROR: " << e.what() << std::endl;
        result.success = false;
        result.errorReason = std::string("Compression error: ") + e.what();
        result.compressionTimeMs = std::numeric_limits<double>::infinity();
    }

    // --- Calculate Ratio ---
    if (result.originalSize > 0) {
        result.ratio = static_cast<double>(result.compressedSize) / result.originalSize;
    }

    return result;
}

int main() {
    std::cout << "🚀 FIXED ULTRA COMPRESSION BENCHMARK - Maximum Ratio Mode" << std::endl;
    std::cout << "===========================================================" << std::endl;
    
    // --- Get Data File Path ---
    std::filesystem::path dataFilePath = "../data/test.txt";

    if (!std::filesystem::exists(dataFilePath)) {
         std::cerr << "Error: Benchmark data file not found at: " << dataFilePath << std::endl;
         return 1;
    }

    // --- Test with different sizes to find the issue ---
    std::vector<size_t> testSizes = {1000, 10000, 100000, 1000000, SIZE_MAX};
    
    for (size_t testSize : testSizes) {
        std::cout << "\n🎯 TESTING WITH SIZE: " << (testSize == SIZE_MAX ? "FULL" : std::to_string(testSize)) << " bytes" << std::endl;
        std::cout << "========================================================" << std::endl;
        
        // --- Read Data ---
        std::vector<uint8_t> originalData;
        try {
            originalData = readFile(dataFilePath, testSize);
        } catch (const std::exception& e) {
            std::cerr << "Failed to read benchmark data: " << e.what() << std::endl;
            continue;
        }

        if (originalData.empty()) {
            std::cerr << "No data to test." << std::endl;
            continue;
        }

        std::cout << "📁 File: " << dataFilePath << std::endl;
        std::cout << "📏 Size: " << originalData.size() << " bytes (" 
                  << std::fixed << std::setprecision(1) 
                  << (originalData.size() / 1024.0 / 1024.0) << " MB)" << std::endl;

        // --- Run BWT test ---
        auto result = runUltraBenchmark("BWT", std::make_unique<compression::BwtCompressor>(), originalData);
        
        std::cout << "📊 RESULT: ";
        if (result.success) {
            std::cout << "✅ SUCCESS - Ratio: " << std::setprecision(1) << (result.ratio * 100.0) << "%" << std::endl;
        } else {
            std::cout << "❌ FAILED - " << result.errorReason << std::endl;
        }
        
        // If this size fails, we can stop testing larger sizes
        if (!result.success) {
            std::cout << "🛑 Stopping at this size due to failure" << std::endl;
            break;
        }
    }

    std::cout << "\n✅ Fixed ultra compression benchmark completed!" << std::endl;

    return 0;
}
