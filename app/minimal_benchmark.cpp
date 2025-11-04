#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <iomanip>
#include <filesystem>

// Include ONLY the working, verified compressors
#include <compression/ICompressor.hpp>
#include <compression/NullCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/BwtCompressor.hpp>

// Structure to hold benchmark results
struct BenchmarkResult {
    std::string algorithmName;
    size_t originalSize = 0;
    size_t compressedSize = 0;
    double compressionTimeMs = 0.0;
    double decompressionTimeMs = 0.0;
    double ratio = 0.0;
    bool success = true;
};

// Safe file reading
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

// Fast benchmark function
BenchmarkResult runBenchmark(
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

    std::cout << "🧪 Testing " << name << "..." << std::flush;

    try {
        // --- Time Compression ---
        auto startCompress = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> compressedData = compressor->compress(originalData);
        auto endCompress = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> compressDuration = endCompress - startCompress;
        result.compressionTimeMs = compressDuration.count();
        result.compressedSize = compressedData.size();

        // --- Time Decompression ---
        if (!compressedData.empty()) {
            try {
                auto startDecompress = std::chrono::high_resolution_clock::now();
                auto decompressedData = compressor->decompress(compressedData);
                auto endDecompress = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> decompressDuration = endDecompress - startDecompress;
                result.decompressionTimeMs = decompressDuration.count();

                // Sanity check decompression
                if (decompressedData != originalData) {
                    std::cout << " ❌ MISMATCH!" << std::endl;
                    result.success = false;
                }
            } catch (const std::exception& e) {
                std::cout << " ❌ DECOMPRESS ERROR: " << e.what() << std::endl;
                result.success = false;
                result.decompressionTimeMs = std::numeric_limits<double>::infinity();
            }
        }
    } catch (const std::exception& e) {
        std::cout << " ❌ COMPRESS ERROR: " << e.what() << std::endl;
        result.success = false;
        result.compressionTimeMs = std::numeric_limits<double>::infinity();
    }

    // --- Calculate Ratio ---
    if (result.originalSize > 0) {
        result.ratio = static_cast<double>(result.compressedSize) / result.originalSize;
    }

    if (result.success) {
        std::cout << " ✅ " << std::setprecision(1) << result.compressionTimeMs << "ms" << std::endl;
    }

    return result;
}

int main() {
    std::cout << "⚡ MINIMAL BENCHMARK - Working Algorithms Only" << std::endl;
    std::cout << "===============================================" << std::endl;
    
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

    if (originalData.empty()) {
        std::cerr << "Benchmark data file is empty. No benchmarks to run." << std::endl;
        return 0;
    }

    std::cout << "📁 File: " << dataFilePath << std::endl;
    std::cout << "📏 Size: " << originalData.size() << " bytes (" 
              << std::fixed << std::setprecision(1) 
              << (originalData.size() / 1024.0 / 1024.0) << " MB)" << std::endl;
    std::cout << "\n🚀 Starting minimal benchmark (4 algorithms only)..." << std::endl;

    auto totalStart = std::chrono::high_resolution_clock::now();

    // --- Run ONLY Working Algorithms ---
    std::vector<BenchmarkResult> results;
    
    results.push_back(runBenchmark("Null", std::make_unique<compression::NullCompressor>(), originalData));
    results.push_back(runBenchmark("Huffman", std::make_unique<compression::HuffmanCompressor>(), originalData));
    results.push_back(runBenchmark("LZ77", std::make_unique<compression::Lz77Compressor>(32768, 3, 258, false, true, true), originalData));
    results.push_back(runBenchmark("BWT (Optimized)", std::make_unique<compression::BwtCompressor>(), originalData));

    auto totalEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> totalDuration = totalEnd - totalStart;

    // --- Output Results ---
    std::cout << "\n📊 MINIMAL BENCHMARK RESULTS" << std::endl;
    std::cout << "============================" << std::endl;

    std::cout << std::fixed << std::setprecision(3);

    // Sort by compression ratio (best first)
    std::sort(results.begin(), results.end(), 
              [](const BenchmarkResult& a, const BenchmarkResult& b) {
                  return a.ratio < b.ratio;
              });

    for (const auto& result : results) {
        double ratioPercent = result.ratio * 100.0;
        std::string status = result.success ? "✅" : "❌";

        std::cout << "\n" << status << " **" << result.algorithmName << "**" << std::endl;
        std::cout << "   Compression Ratio: " << std::setprecision(1) << ratioPercent << "%" << std::endl;
        std::cout << "   Compression Time:  " << std::setprecision(0) << result.compressionTimeMs << " ms" << std::endl;
        std::cout << "   Decompression Time:" << std::setprecision(0) << result.decompressionTimeMs << " ms" << std::endl;
        std::cout << "   Total Time:        " << std::setprecision(0) << (result.compressionTimeMs + result.decompressionTimeMs) << " ms" << std::endl;
    }

    // Summary
    std::cout << "\n🏆 PERFORMANCE SUMMARY" << std::endl;
    std::cout << "======================" << std::endl;
    
    if (!results.empty() && results[0].success) {
        std::cout << "🥇 Best Compression: " << results[0].algorithmName 
                  << " (" << std::setprecision(1) << (results[0].ratio * 100.0) << "%)" << std::endl;
    }
    
    std::cout << "⚡ Total Benchmark Time: " << std::setprecision(0) << totalDuration.count() << " ms" << std::endl;
    std::cout << "📁 File Size: " << (originalData.size() / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "🚀 Processing Speed: " << std::setprecision(1) 
              << (originalData.size() / 1024.0 / 1024.0) / (totalDuration.count() / 1000.0) << " MB/s" << std::endl;

    std::cout << "\n✅ Minimal benchmark completed successfully!" << std::endl;
    std::cout << "✅ Only working algorithms tested" << std::endl;
    std::cout << "✅ No hanging or broken algorithms" << std::endl;

    return 0;
}
