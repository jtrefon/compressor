#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <iomanip>
#include <filesystem>

// Include compressor headers
#include <compression/ICompressor.hpp>
#include <compression/NullCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/ArithmeticCompressor.hpp>
#include <compression/OptimizedCompressor.hpp>
#include <compression/HybridCompressor.hpp>

// Structure to hold benchmark results
struct BenchmarkResult {
    std::string algorithmName;
    size_t originalSize = 0;
    size_t compressedSize = 0;
    double compressionTimeMs = 0.0;
    double decompressionTimeMs = 0.0;
    double ratio = 0.0;
};

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

// Runs compress/decompress and times them
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
                    std::cerr << "WARNING: Decompression mismatch for " << name << "!" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "ERROR: Decompression failed for " << name << ": " << e.what() << std::endl;
                result.decompressionTimeMs = std::numeric_limits<double>::infinity();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Compression failed for " << name << ": " << e.what() << std::endl;
        result.compressionTimeMs = std::numeric_limits<double>::infinity();
    }

    // --- Calculate Ratio ---
    if (result.originalSize > 0) {
        result.ratio = static_cast<double>(result.compressedSize) / result.originalSize;
    }

    return result;
}

int main() {
    // --- Get Data File Path ---
    std::filesystem::path dataFilePath = "../data/test.txt";

    // Check if the file exists
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

    std::cout << "Starting simple benchmark using file: " << dataFilePath << std::endl;
    std::cout << "Read " << originalData.size() << " bytes." << std::endl;

    // --- Run Benchmarks ---
    std::vector<BenchmarkResult> results;
    
    // Test basic compressors first
    results.push_back(runBenchmark("Null", std::make_unique<compression::NullCompressor>(), originalData));
    results.push_back(runBenchmark("Huffman", std::make_unique<compression::HuffmanCompressor>(), originalData));
    results.push_back(runBenchmark("LZ77", std::make_unique<compression::Lz77Compressor>(32768, 3, 258, false, true, true), originalData));
    results.push_back(runBenchmark("Arithmetic", std::make_unique<compression::ArithmeticCompressor>(), originalData));
    results.push_back(runBenchmark("Optimized", std::make_unique<compression::OptimizedCompressor>(), originalData));
    results.push_back(runBenchmark("Hybrid", std::make_unique<compression::HybridCompressor>(), originalData));

    // --- Output Results ---
    std::cout << "\n--- Simple Benchmark Results ---\n" << std::endl;

    std::cout << std::fixed << std::setprecision(3);

    for (const auto& result : results) {
        double ratioPercent = result.ratio * 100.0;

        // Console Output
        std::cout << "Algorithm:       " << result.algorithmName << std::endl;
        std::cout << "Original Size:   " << result.originalSize << " bytes" << std::endl;
        std::cout << "Compressed Size: " << result.compressedSize << " bytes" << std::endl;
        std::cout << "Ratio:           " << std::setprecision(2) << ratioPercent << "%" << std::endl;
        std::cout << "Compress Time:   " << std::setprecision(3) << result.compressionTimeMs << " ms" << std::endl;
        std::cout << "Decompress Time: " << result.decompressionTimeMs << " ms" << std::endl;
        std::cout << "-------------------------" << std::endl;
    }

    return 0;
}
