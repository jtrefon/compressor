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

    std::cout << "🚀 Testing " << name << "..." << std::flush;

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
    std::cout << "🚀 ULTRA COMPRESSION BENCHMARK - Maximum Ratio Mode" << std::endl;
    std::cout << "=====================================================" << std::endl;
    
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
    std::cout << "\n⏳ Starting ultra compression benchmark (this will take time)..." << std::endl;

    auto totalStart = std::chrono::high_resolution_clock::now();

    // --- Run ALL Algorithms Including Ultra Ones ---
    std::vector<BenchmarkResult> results;
    
    // Baseline algorithms
    results.push_back(runUltraBenchmark("Null", std::make_unique<compression::NullCompressor>(), originalData));
    results.push_back(runUltraBenchmark("Huffman", std::make_unique<compression::HuffmanCompressor>(), originalData));
    results.push_back(runUltraBenchmark("LZ77", std::make_unique<compression::Lz77Compressor>(32768, 3, 258, false, true, true), originalData));
    results.push_back(runUltraBenchmark("BWT", std::make_unique<compression::BwtCompressor>(), originalData));
    
    // Ultra compression algorithms
    results.push_back(runUltraBenchmark("Ultra (BWT+LZ77)", std::make_unique<compression::UltraCompressor>(), originalData));
    results.push_back(runUltraBenchmark("Extreme (Adaptive)", std::make_unique<compression::ExtremeCompressor>(), originalData));

    auto totalEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> totalDuration = totalEnd - totalStart;

    // --- Output Results ---
    std::cout << "\n📊 ULTRA COMPRESSION RESULTS" << std::endl;
    std::cout << "=============================" << std::endl;

    std::cout << std::fixed << std::setprecision(3);

    // Sort by compression ratio (best first)
    std::sort(results.begin(), results.end(), 
              [](const BenchmarkResult& a, const BenchmarkResult& b) {
                  return a.ratio < b.ratio;
              });

    for (const auto& result : results) {
        double ratioPercent = result.ratio * 100.0;
        std::string status = result.success ? "✅" : "❌";

        std::cout << status << " **" << result.algorithmName << "**" << std::endl;
        std::cout << "   Compression Ratio: " << std::setprecision(1) << ratioPercent << "%" << std::endl;
        std::cout << "   Compressed Size:   " << result.compressedSize << " bytes" << std::endl;
        std::cout << "   Compression Time:  " << std::setprecision(0) << result.compressionTimeMs << " ms" << std::endl;
        std::cout << "   Decompression Time:" << std::setprecision(0) << result.decompressionTimeMs << " ms" << std::endl;
        std::cout << "   Total Time:        " << std::setprecision(0) << (result.compressionTimeMs + result.decompressionTimeMs) << " ms" << std::endl;
        
        if (!result.success) {
            std::cout << "   Error: " << result.errorReason << std::endl;
        }
        
        // Calculate space savings
        double spaceSavings = (1.0 - result.ratio) * 100.0;
        std::cout << "   Space Savings:     " << std::setprecision(1) << spaceSavings << "%" << std::endl;
        std::cout << std::endl;
    }

    // Summary
    std::cout << "🏆 ULTRA COMPRESSION SUMMARY" << std::endl;
    std::cout << "============================" << std::endl;
    
    if (!results.empty() && results[0].success) {
        std::cout << "🥇 Best Compression: " << results[0].algorithmName 
                  << " (" << std::setprecision(1) << (results[0].ratio * 100.0) << "%)" << std::endl;
        
        double spaceSavings = (1.0 - results[0].ratio) * 100.0;
        std::cout << "💾 Maximum Space Savings: " << std::setprecision(1) << spaceSavings << "%" << std::endl;
        
        size_t bytesSaved = originalData.size() - results[0].compressedSize;
        std::cout << "📊 Bytes Saved: " << bytesSaved << " bytes (" 
                  << std::setprecision(1) << (bytesSaved / 1024.0 / 1024.0) << " MB)" << std::endl;
    }
    
    std::cout << "⏱️  Total Benchmark Time: " << std::setprecision(0) << totalDuration.count() << " ms" << std::endl;
    std::cout << "📁 Original File Size: " << (originalData.size() / 1024.0 / 1024.0) << " MB" << std::endl;

    std::cout << "\n🎯 RECOMMENDATIONS" << std::endl;
    std::cout << "==================" << std::endl;
    
    // Find best trade-offs
    auto bestRatio = results[0];
    auto fastestCompress = *std::min_element(results.begin(), results.end(),
        [](const BenchmarkResult& a, const BenchmarkResult& b) {
            return a.compressionTimeMs < b.compressionTimeMs && a.success;
        });
    
    std::cout << "🏆 For Maximum Compression: Use " << bestRatio.algorithmName 
              << " (" << std::setprecision(1) << (bestRatio.ratio * 100.0) << "%)" << std::endl;
    std::cout << "⚡ For Fastest Compression: Use " << fastestCompress.algorithmName 
              << " (" << std::setprecision(0) << fastestCompress.compressionTimeMs << " ms)" << std::endl;

    std::cout << "\n✅ Ultra compression benchmark completed!" << std::endl;
    std::cout << "✅ Maximum compression ratios achieved!" << std::endl;

    return 0;
}
