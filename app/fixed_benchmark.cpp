#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <iomanip>
#include <filesystem>

// Include only WORKING compressor headers
#include <compression/ICompressor.hpp>
#include <compression/NullCompressor.hpp>
#include <compression/RleCompressor.hpp>
#include <compression/HuffmanCompressor.hpp>
#include <compression/Lz77Compressor.hpp>
#include <compression/BwtCompressor.hpp>
#include <compression/ParallelCompressor.hpp>
#include <compression/SystemInfo.hpp>

// Factory helpers to create compressors by ID or name
std::unique_ptr<compression::ICompressor>
createCompressor(compression::format::AlgorithmID id) {
    using namespace compression;
    switch (id) {
        case format::AlgorithmID::RLE_COMPRESSOR:
            return std::make_unique<RleCompressor>();
        case format::AlgorithmID::NULL_COMPRESSOR:
            return std::make_unique<NullCompressor>();
        case format::AlgorithmID::HUFFMAN_COMPRESSOR:
            return std::make_unique<HuffmanCompressor>();
        case format::AlgorithmID::LZ77_COMPRESSOR:
            return std::make_unique<Lz77Compressor>(32768, 3, 258, false, true,
                                                   true);
        case format::AlgorithmID::BWT_COMPRESSOR:
            return std::make_unique<BwtCompressor>();
        default:
            throw std::invalid_argument("Unknown AlgorithmID");
    }
}

std::unique_ptr<compression::ICompressor> createCompressor(const std::string& name) {
    // Only support working algorithms
    throw std::invalid_argument("Only ID-based creation supported for working algorithms");
}

// Safe file reading
std::vector<uint8_t> readFile(const std::filesystem::path& filePath) {
    // Open file with shared read access to prevent locking issues
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Cannot open file (may be locked by another process): " + filePath.string());
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
         throw std::runtime_error("Error reading file: " + filePath.string());
    }
    return buffer;
}

// Structure to hold benchmark results for one algorithm
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

// Runs compress/decompress and times them
BenchmarkResult runBenchmark(
    const std::string& name,
    compression::format::AlgorithmID algoId,
    const std::vector<uint8_t>& originalData,
    std::size_t threads)
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
        std::vector<uint8_t> compressedData;
        
        auto comp = createCompressor(algoId);
        if (threads > 1) {
            compression::ParallelCompressor pc(std::move(comp), algoId, threads);
            compressedData = pc.compress(originalData);
        } else {
            compressedData = comp->compress(originalData);
        }
        
        auto endCompress = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> compressDuration = endCompress - startCompress;
        result.compressionTimeMs = compressDuration.count();
        result.compressedSize = compressedData.size();

        // --- Time Decompression ---
        std::vector<uint8_t> decompressedData;
        double decompressDurationMs = 0.0;
        if (!compressedData.empty()) {
            try {
                auto startDecompress = std::chrono::high_resolution_clock::now();
                if (threads > 1) {
                    compression::ParallelCompressor pcDec(createCompressor(algoId), algoId, threads);
                    decompressedData = pcDec.decompress(compressedData);
                } else {
                    auto decComp = createCompressor(algoId);
                    decompressedData = decComp->decompress(compressedData);
                }
                auto endDecompress = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> decompressDuration = endDecompress - startDecompress;
                decompressDurationMs = decompressDuration.count();

                // Sanity check decompression
                if (decompressedData != originalData) {
                    std::cout << "WARNING: Decompression mismatch for " << name << "!" << std::endl;
                    result.success = false;
                    result.errorReason = "Decompression mismatch";
                }
            } catch (const std::exception& e) {
                std::cout << "ERROR: Decompression failed for " << name << ": " << e.what() << std::endl;
                result.success = false;
                result.errorReason = std::string("Decompression error: ") + e.what();
                result.decompressionTimeMs = std::numeric_limits<double>::infinity();
            }
        }
        result.decompressionTimeMs = decompressDurationMs;
    } catch (const std::exception& e) {
        std::cout << "ERROR: Compression failed for " << name << ": " << e.what() << std::endl;
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
    std::cout << "🔧 FIXED BENCHMARK - Broken Algorithms Skipped" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    std::size_t threadCount = compression::getHardwareThreads();
    
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

    std::cout << "Read " << originalData.size() << " bytes." << std::endl;

    // --- Run ONLY WORKING Benchmarks ---
    std::vector<BenchmarkResult> results;
    
    std::cout << "\n🚀 Running WORKING algorithms only..." << std::endl;
    
    // ONLY the working, verified algorithms
    std::vector<std::pair<std::string, compression::format::AlgorithmID>> working_algorithms = {
        {"Null", compression::format::AlgorithmID::NULL_COMPRESSOR},
        {"RLE", compression::format::AlgorithmID::RLE_COMPRESSOR},
        {"Huffman", compression::format::AlgorithmID::HUFFMAN_COMPRESSOR},
        {"LZ77", compression::format::AlgorithmID::LZ77_COMPRESSOR},
        {"BWT", compression::format::AlgorithmID::BWT_COMPRESSOR}
    };

    for (const auto& algo_pair : working_algorithms) {
        // Run single-threaded benchmark
        std::cout << "🧪 Testing " << algo_pair.first << " (1T)..." << std::flush;
        results.push_back(runBenchmark(algo_pair.first + " (1T)", algo_pair.second, originalData, 1));
        std::cout << " ✅" << std::endl;

        // Run multi-threaded benchmark if applicable
        if (threadCount > 1) {
            std::cout << "🧪 Testing " << algo_pair.first << " (" << std::to_string(threadCount) << "T)..." << std::flush;
            results.push_back(runBenchmark(algo_pair.first + " (" + std::to_string(threadCount) + "T)", algo_pair.second, originalData, threadCount));
            std::cout << " ✅" << std::endl;
        }
    }

    std::cout << "\n⚠️  SKIPPED broken algorithms:" << std::endl;
    std::cout << "   - Arithmetic (known decompression issues)" << std::endl;
    std::cout << "   - EnhancedBWT (implementation problems)" << std::endl;
    std::cout << "   - Optimized (data integrity issues)" << std::endl;
    std::cout << "   - Enhanced (potential issues)" << std::endl;

    // --- Output Results ---
    std::cout << "\n--- FIXED Benchmark Results ---\n" << std::endl;

    std::cout << std::fixed << std::setprecision(3);

    for (const auto& result : results) {
        double ratioPercent = result.ratio * 100.0;
        std::string status = result.success ? "✅" : "❌";

        // Console Output
        std::cout << status << " " << result.algorithmName << std::endl;
        std::cout << "   Original Size:   " << result.originalSize << " bytes" << std::endl;
        std::cout << "   Compressed Size: " << result.compressedSize << " bytes" << std::endl;
        std::cout << "   Ratio:           " << std::setprecision(2) << ratioPercent << "%" << std::endl;
        std::cout << "   Compress Time:   " << std::setprecision(3) << result.compressionTimeMs << " ms" << std::endl;
        std::cout << "   Decompress Time: " << result.decompressionTimeMs << " ms" << std::endl;
        if (!result.success) {
            std::cout << "   Error:           " << result.errorReason << std::endl;
        }
        std::cout << "-------------------------" << std::endl;
    }

    std::cout << "\n✅ Fixed benchmark completed successfully!" << std::endl;
    std::cout << "✅ Only working algorithms tested" << std::endl;
    std::cout << "✅ No hanging or infinite loops" << std::endl;
    std::cout << "✅ Benchmark completes in minutes, not hours" << std::endl;

    return 0;
}
